#include "ProcessFlowModel.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

using namespace mpp;

namespace pipeline_editor
{
	namespace
	{
		uint64_t hashText(std::string const& text, uint64_t seed = 1469598103934665603ull)
		{
			for (unsigned char value : text) { seed ^= value; seed *= 1099511628211ull; }
			return seed ? seed : 1;
		}

		std::string imageKey(GraphImageHandle image)
		{
			return std::to_string(image.id) + ":" + std::to_string(image.version);
		}

		bool depthFormat(GraphImageFormat format)
		{
			return format == GraphImageFormat::Depth16 || format == GraphImageFormat::Depth24 ||
			       format == GraphImageFormat::Depth32f || format == GraphImageFormat::Depth24Stencil8 ||
			       format == GraphImageFormat::Depth32fStencil8;
		}

		bool containsInsensitive(std::string value, char const* needle)
		{
			std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return (char)std::tolower(c); });
			return value.find(needle) != std::string::npos;
		}

		ProcessFlowResourceCategory eventCategory(RenderFlowEventKind kind)
		{
			switch (kind)
			{
			case RenderFlowEventKind::MsaaResolve: return ProcessFlowResourceCategory::MsaaResources;
			case RenderFlowEventKind::Taa: return ProcessFlowResourceCategory::TaaHistories;
			case RenderFlowEventKind::SsaaHorizontal:
			case RenderFlowEventKind::SsaaVertical: return ProcessFlowResourceCategory::SsaaTargets;
			case RenderFlowEventKind::Fxaa: return ProcessFlowResourceCategory::FxaaTargets;
			default: return ProcessFlowResourceCategory::None;
			}
		}

		ProcessFlowNodeKind eventNodeKind(RenderFlowEventKind kind)
		{
			switch (kind)
			{
			case RenderFlowEventKind::MsaaResolve: return ProcessFlowNodeKind::MsaaResolve;
			case RenderFlowEventKind::Taa: return ProcessFlowNodeKind::Taa;
			case RenderFlowEventKind::SsaaHorizontal:
			case RenderFlowEventKind::SsaaVertical: return ProcessFlowNodeKind::Ssaa;
			case RenderFlowEventKind::Fxaa: return ProcessFlowNodeKind::Fxaa;
			default: return ProcessFlowNodeKind::Presentation;
			}
		}

		std::string resourceDescription(RenderFlowResourceDesc const& resource)
		{
			return std::to_string(resource.size.x) + " x " + std::to_string(resource.size.y) + ", " +
			       graphImageFormatName(resource.format) + ", " + std::to_string(resource.samples) + " sample(s)";
		}
	}

	bool ProcessFlowFilters::visible(ProcessFlowResourceCategory category) const
	{
		return category != ProcessFlowResourceCategory::None &&
		       (resources & static_cast<uint32_t>(category)) != 0;
	}

	ProcessFlowNode* ProcessFlowModel::findNode(uint64_t id)
	{
		auto found = std::find_if(nodes.begin(), nodes.end(), [&](auto const& node) { return node.id == id; });
		return found == nodes.end() ? nullptr : &*found;
	}
	ProcessFlowNode const* ProcessFlowModel::findNode(uint64_t id) const
	{
		auto found = std::find_if(nodes.begin(), nodes.end(), [&](auto const& node) { return node.id == id; });
		return found == nodes.end() ? nullptr : &*found;
	}

	ProcessFlowModel ProcessFlowModelBuilder::build(ProcessFlowBuildInput const& input)
	{
		ProcessFlowModel model;
		model.revision = ++mRevision;
		if (!input.graph)
		{
			model.diagnostics.push_back("No active pipeline generation.");
			return model;
		}
		auto compiled = input.graph->compile();
		if (!compiled.valid)
		{
			model.diagnostics = compiled.diagnostics;
			if (model.diagnostics.empty()) model.diagnostics.push_back("Render-graph dependency compilation failed.");
			return model;
		}
		model.pipelineGeneration = input.snapshot ? input.snapshot->pipelineGeneration : 0;
		model.frameSerial = input.snapshot ? input.snapshot->frameSerial : 0;
		model.liveSample = !!input.snapshot;
		auto stableId = [&](std::string const& key) { return hashText(key, hashText(std::to_string(model.pipelineGeneration))); };
		std::unordered_map<uint64_t, size_t> nodeIndices;
		auto addNode = [&](ProcessFlowNode node) -> uint64_t
		{
			node.id = stableId(node.semanticKey);
			while (nodeIndices.contains(node.id)) node.id = hashText(node.semanticKey, node.id);
			nodeIndices[node.id] = model.nodes.size();
			model.nodes.push_back(std::move(node));
			return model.nodes.back().id;
		};
		auto addEdge = [&](uint64_t source, uint64_t destination, ProcessFlowEdgeKind kind, std::string label)
		{
			if (!source || !destination || source == destination) return;
			auto key = std::to_string(source) + ">" + std::to_string(destination) + ":" +
			           std::to_string((int)kind) + ":" + label;
			uint64_t id = stableId("edge:" + key);
			if (std::any_of(model.edges.begin(), model.edges.end(), [&](auto const& edge) { return edge.id == id; })) return;
			model.edges.push_back({id, source, destination, kind, std::move(label)});
		};

		std::vector<GraphPassHandle> actual = input.snapshot ? input.snapshot->actualPassOrder : compiled.passOrder;
		std::unordered_map<uint32_t, int> actualPositions;
		for (size_t index = 0; index < actual.size(); ++index) actualPositions[actual[index].id] = (int)index;
		std::vector<uint64_t> passNodes(input.graph->getPassCount());
		for (uint32_t pass = 0; pass < input.graph->getPassCount(); ++pass)
		{
			auto info = input.graph->getPassInfo({pass});
			ProcessFlowNode node;
			node.semanticKey = "pass:" + std::to_string(pass);
			node.title = info.name;
			node.subtitle = info.callbackFactory.empty() ? "Authored pass" : info.callbackFactory;
			node.kind = ProcessFlowNodeKind::AuthoredPass;
			node.passId = (int)pass;
			node.authoredPosition = (int)pass;
			auto position = actualPositions.find(pass);
			node.actualPosition = position == actualPositions.end() ? -1 : position->second;
			node.enabled = position != actualPositions.end();
			if (pass < input.passBypassReasons.size() && !input.passBypassReasons[pass].empty())
			{
				node.enabled = false;
				node.bypassReason = input.passBypassReasons[pass];
			}
			else if (!info.enabled)
			{
				node.enabled = false;
				node.bypassReason = "Disabled by authored pass setting";
			}
			else if (!node.enabled) node.bypassReason = "Not present in the last successful compiled execution order";
			node.orderWarning = node.enabled && node.actualPosition != node.authoredPosition;
			node.layoutRank = node.actualPosition >= 0 ? (float)node.actualPosition : (float)node.authoredPosition;
			passNodes[pass] = addNode(std::move(node));
		}

		std::vector<uint64_t> spine;
		if (input.snapshot)
		{
			std::unordered_map<uint64_t, RenderBatchSubmission const*> batches;
			for (auto const& batch : input.snapshot->batches) batches[batch.sequence] = &batch;
			for (auto const& event : input.snapshot->physicalEvents)
			{
				if (event.kind == RenderFlowEventKind::PassBegin)
				{
					if (event.pass.id < passNodes.size()) { auto id = passNodes[event.pass.id]; model.findNode(id)->mainSpine = true; model.findNode(id)->sequence = event.sequence; spine.push_back(id); }
					continue;
				}
				if (event.kind == RenderFlowEventKind::PassEnd) continue;
				if (event.kind == RenderFlowEventKind::BatchSubmission)
				{
					auto found = batches.find(event.sequence);
					if (found == batches.end()) throw std::runtime_error("Flow event references a missing batch submission.");
					auto const& batch = *found->second;
					ProcessFlowNode node;
					node.semanticKey = "batch:" + std::to_string(batch.sequence);
					node.title = batch.meshName.empty() ? "Submitted batch" : batch.meshName;
					node.subtitle = batch.materialName.empty() ? batch.programName : batch.materialName;
					node.details = std::to_string(batch.count) + " primitives, " + std::to_string(batch.instanceCount) + " instance(s)";
					node.kind = ProcessFlowNodeKind::BatchSubmission;
					node.sequence = batch.sequence;
					node.passId = (int)batch.parentPass.id;
					node.materialName = batch.materialName;
					if (batch.sceneObject) node.sceneObjects.push_back(batch.sceneObject);
					node.mainSpine = true;
					node.layoutRank = (float)batch.sequence;
					spine.push_back(addNode(std::move(node)));
					continue;
				}
				ProcessFlowNode node;
				node.semanticKey = "event:" + std::to_string(event.sequence) + ":" + std::to_string((int)event.kind);
				node.title = event.name.empty() ? renderFlowEventKindName(event.kind) : event.name;
				node.subtitle = renderFlowEventKindName(event.kind);
				node.kind = eventNodeKind(event.kind);
				node.sequence = event.sequence;
				node.passId = event.pass.isValid() ? (int)event.pass.id : -1;
				node.enabled = event.enabled;
				node.bypassReason = event.bypassReason;
				node.mainSpine = event.enabled;
				node.layoutRank = (float)event.sequence;
				auto eventId = addNode(std::move(node));
				if (event.enabled) spine.push_back(eventId);
			}
		}
		else
			for (auto pass : actual) { model.findNode(passNodes[pass.id])->mainSpine = true; spine.push_back(passNodes[pass.id]); }
		if (input.filters.executionEdges)
			for (size_t index = 1; index < spine.size(); ++index)
				addEdge(spine[index - 1], spine[index], ProcessFlowEdgeKind::Execution, {});

		if (input.filters.resourceEdges)
		{
			std::unordered_map<std::string, uint64_t> producers;
			for (uint32_t pass = 0; pass < input.graph->getPassCount(); ++pass)
			{
				auto info = input.graph->getPassInfo({pass});
				for (auto const& output : info.colourOutputs) producers[imageKey(output.image)] = passNodes[pass];
				for (auto const& output : info.depthOutputs) producers[imageKey(output.image)] = passNodes[pass];
			}
			std::unordered_map<std::string, uint64_t> authoredResources;
			auto dependency = [&](GraphImageHandle image, uint64_t consumer, uint32_t mip)
			{
				auto info = input.graph->getImageInfo(image);
				auto key = imageKey(image);
				auto producer = producers.find(key);
				bool imported = info.desc.external || !info.importName.empty();
				auto kind = imported ? ProcessFlowEdgeKind::Import : depthFormat(info.desc.format)
				            ? (containsInsensitive(info.name, "shadow") ? ProcessFlowEdgeKind::Shadow : ProcessFlowEdgeKind::Depth)
				            : ProcessFlowEdgeKind::Colour;
				auto label = info.name + ".v" + std::to_string(image.version);
				if (mip != UINT32_MAX) label += " mip " + std::to_string(mip);
				auto category = imported ? ProcessFlowResourceCategory::Imports : ProcessFlowResourceCategory::AuthoredImages;
				if (input.filters.visible(category))
				{
					uint64_t resourceId = authoredResources[key];
					if (!resourceId)
					{
						ProcessFlowNode node;
						node.semanticKey = (imported ? "import:" : "image:") + key;
						node.title = imported && !info.importName.empty() ? info.importName : label;
						node.subtitle = std::string(graphImageFormatName(info.desc.format)) + (imported ? " import" : " authored image");
						node.kind = imported ? ProcessFlowNodeKind::Import : ProcessFlowNodeKind::AuthoredImage;
						node.resourceCategory = category;
						node.imageId = (int)image.id;
						node.layoutRank = model.findNode(consumer)->layoutRank - 0.25f;
						resourceId = authoredResources[key] = addNode(std::move(node));
					}
					if (producer != producers.end()) addEdge(producer->second, resourceId, kind, label);
					addEdge(resourceId, consumer, kind, label);
				}
				else if (producer != producers.end()) addEdge(producer->second, consumer, kind, label);
			};
			for (uint32_t pass = 0; pass < input.graph->getPassCount(); ++pass)
			{
				auto info = input.graph->getPassInfo({pass});
				std::set<std::string> seen;
				for (auto image : info.sampledInputs)
					if (seen.insert(imageKey(image) + ":all").second) dependency(image, passNodes[pass], UINT32_MAX);
				for (auto const& binding : info.samplerBindings)
					if (seen.insert(imageKey(binding.image) + ":" + std::to_string(binding.mipLevel)).second)
						dependency(binding.image, passNodes[pass], binding.mipLevel);
			}

			if (input.snapshot)
			{
				struct PhysicalProducer { uint64_t stage{ 0 }, resource{ 0 }; };
				std::unordered_map<std::string, PhysicalProducer> physical;
				std::unordered_map<std::string, uint64_t> resolvedImages;
				for (auto const& event : input.snapshot->physicalEvents)
				{
					if (event.kind == RenderFlowEventKind::PassBegin || event.kind == RenderFlowEventKind::PassEnd ||
					    event.kind == RenderFlowEventKind::BatchSubmission) continue;
					auto stageId = stableId("event:" + std::to_string(event.sequence) + ":" + std::to_string((int)event.kind));
					if (!model.findNode(stageId)) continue;
					auto category = eventCategory(event.kind);
					auto edgeKind = event.kind == RenderFlowEventKind::Taa ? ProcessFlowEdgeKind::History :
					                event.kind == RenderFlowEventKind::Presentation ? ProcessFlowEdgeKind::Output : ProcessFlowEdgeKind::Colour;
					for (size_t index = 0; index < event.inputs.size(); ++index)
					{
						auto const& value = event.inputs[index];
						auto found = physical.find(value.name);
						if (input.filters.visible(category))
						{
							uint64_t resourceId = found == physical.end() ? 0 : found->second.resource;
							if (!resourceId)
							{
								ProcessFlowNode node;
								node.semanticKey = "physical-in:" + std::to_string(event.sequence) + ":" + std::to_string(index);
								node.title = value.name; node.subtitle = resourceDescription(value);
								node.kind = category == ProcessFlowResourceCategory::TaaHistories ? ProcessFlowNodeKind::TaaHistory : ProcessFlowNodeKind::PhysicalWorkTarget;
								node.resourceCategory = category; node.layoutRank = model.findNode(stageId)->layoutRank - 0.2f;
								resourceId = addNode(std::move(node));
								if (found != physical.end()) found->second.resource = resourceId;
							}
							if (found != physical.end() && found->second.stage) addEdge(found->second.stage, resourceId, edgeKind, value.name);
							if (event.kind == RenderFlowEventKind::MsaaResolve && event.image.isValid())
							{
								auto graphProducer = producers.find(imageKey(event.image));
								if (graphProducer != producers.end()) addEdge(graphProducer->second, resourceId, event.depth ? ProcessFlowEdgeKind::Depth : ProcessFlowEdgeKind::Colour, event.name);
							}
							addEdge(resourceId, stageId, edgeKind, value.name);
						}
						else if (found != physical.end()) addEdge(found->second.resource ? found->second.resource : found->second.stage, stageId, edgeKind, value.name);
						else if (event.kind == RenderFlowEventKind::MsaaResolve && event.image.isValid())
						{
							auto graphProducer = producers.find(imageKey(event.image));
							if (graphProducer != producers.end()) addEdge(graphProducer->second, stageId, event.depth ? ProcessFlowEdgeKind::Depth : ProcessFlowEdgeKind::Colour, event.name);
						}
					}
					for (size_t index = 0; index < event.outputs.size(); ++index)
					{
						auto const& value = event.outputs[index]; uint64_t resourceId = 0;
						if (input.filters.visible(category))
						{
							ProcessFlowNode node; node.semanticKey = "physical-out:" + std::to_string(event.sequence) + ":" + std::to_string(index);
							node.title = value.name; node.subtitle = resourceDescription(value);
							node.kind = category == ProcessFlowResourceCategory::TaaHistories ? ProcessFlowNodeKind::TaaHistory : ProcessFlowNodeKind::PhysicalWorkTarget;
							node.resourceCategory = category; node.layoutRank = model.findNode(stageId)->layoutRank + 0.2f;
							resourceId = addNode(std::move(node)); addEdge(stageId, resourceId, edgeKind, value.name);
						}
						physical[value.name] = {stageId, resourceId};
						if (event.kind == RenderFlowEventKind::MsaaResolve && event.image.isValid())
							resolvedImages[imageKey(event.image)] = resourceId ? resourceId : stageId;
					}
				}
				for (auto const& plan : input.snapshot->outputPlans)
				{
					uint64_t firstStage = 0;
					for (auto const& event : input.snapshot->physicalEvents)
						if (event.enabled && event.outputName == plan.name && event.kind != RenderFlowEventKind::MsaaResolve)
						{ firstStage = stableId("event:" + std::to_string(event.sequence) + ":" + std::to_string((int)event.kind)); break; }
					if (!firstStage || !model.findNode(firstStage)) continue;
					for (uint32_t image = 0; image < input.graph->getImageCount(); ++image)
					{
						auto info = input.graph->getImageInfo({image, 0});
						if (info.name != plan.image && (plan.taaDepth.empty() || info.name != plan.taaDepth)) continue;
						GraphImageHandle handle{image, (uint32_t)input.graph->getImageVersionCount(image) - 1};
						auto resolved = resolvedImages.find(imageKey(handle));
						if (resolved != resolvedImages.end())
							addEdge(resolved->second, firstStage, depthFormat(info.desc.format) ? ProcessFlowEdgeKind::Depth : ProcessFlowEdgeKind::Colour,
							        info.name + ".v" + std::to_string(handle.version));
						else dependency(handle, firstStage, UINT32_MAX);
					}
				}
				if (input.filters.visible(ProcessFlowResourceCategory::NamedOutputs))
					for (size_t index = 0; index < input.snapshot->outputPlans.size(); ++index)
					{
						auto const& plan = input.snapshot->outputPlans[index];
						ProcessFlowNode node; node.semanticKey = "output:" + plan.name; node.title = plan.name;
						node.subtitle = std::to_string(plan.logicalSize.x) + " x " + std::to_string(plan.logicalSize.y) + " named output";
						node.details = "MSAA " + antiAliasingSamplesName(plan.antiAliasing.msaa) + ", SSAA " +
						               antiAliasingSamplesName(plan.antiAliasing.ssaa) + ", TAA " +
						               (plan.antiAliasing.taa ? "on" : "off") + ", FXAA " +
						               (plan.antiAliasing.fxaa ? "on" : "off");
						node.kind = ProcessFlowNodeKind::NamedOutput; node.resourceCategory = ProcessFlowResourceCategory::NamedOutputs;
						node.layoutRank = 100000.0f + (float)index; auto id = addNode(std::move(node));
						for (auto iterator = input.snapshot->physicalEvents.rbegin(); iterator != input.snapshot->physicalEvents.rend(); ++iterator)
							if (iterator->kind == RenderFlowEventKind::Presentation && iterator->outputName == plan.name)
							{ auto source = stableId("event:" + std::to_string(iterator->sequence) + ":" + std::to_string((int)iterator->kind)); addEdge(source, id, ProcessFlowEdgeKind::Output, plan.name); break; }
					}
			}
		}
		model.largeGraph = model.nodes.size() > 500;
		return model;
	}

	void runProcessFlowModelTests()
	{
		RenderGraph graph;
		GraphImageDesc desc; desc.usage = GraphImageUsage::ColourAttachment | GraphImageUsage::Sampled;
		auto image = graph.createImage("FlowColour", desc);
		auto producer = graph.addPass("Producer"); auto produced = graph.writeColour(producer, image);
		auto consumer = graph.addPass("Consumer"); graph.readSampled(consumer, produced);
		auto snapshot = std::make_shared<RenderPipelineFlowSnapshot>(); snapshot->pipelineGeneration = 9;
		snapshot->actualPassOrder = {consumer, producer};
		RenderBatchSubmission first; first.sequence = 2; first.parentPass = consumer; first.meshName = "Duplicate";
		auto second = first; second.sequence = 3; snapshot->batches = {first, second};
		snapshot->physicalEvents = {{RenderFlowEventKind::PassBegin, 1, consumer}, {RenderFlowEventKind::BatchSubmission, 2, consumer}, {RenderFlowEventKind::BatchSubmission, 3, consumer}, {RenderFlowEventKind::PassEnd, 4, consumer}, {RenderFlowEventKind::PassBegin, 5, producer}, {RenderFlowEventKind::PassEnd, 6, producer}};
		ProcessFlowModelBuilder builder; ProcessFlowBuildInput input{&graph, snapshot};
		auto model = builder.build(input);
		if (std::count_if(model.nodes.begin(), model.nodes.end(), [](auto const& node) { return node.kind == ProcessFlowNodeKind::BatchSubmission; }) != 2)
			throw std::runtime_error("Process-flow model aggregated duplicate submissions.");
		if (!model.findNode(model.nodes[0].id) || !std::any_of(model.nodes.begin(), model.nodes.end(), [](auto const& node) { return node.orderWarning; }))
			throw std::runtime_error("Process-flow model stable identity/order warning test failed.");
		input.filters.resources = (uint32_t)ProcessFlowResourceCategory::AuthoredImages;
		auto resources = builder.build(input);
		if (std::none_of(resources.nodes.begin(), resources.nodes.end(), [](auto const& node) { return node.kind == ProcessFlowNodeKind::AuthoredImage; }))
			throw std::runtime_error("Process-flow authored-resource transformation test failed.");
	}
}
