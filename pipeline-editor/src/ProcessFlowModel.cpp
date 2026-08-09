#include "ProcessFlowModel.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

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

	bool ProcessFlowSampleGate::poll(double timestampSeconds, bool force)
	{
		if (!force && mLastPoll >= 0.0 && timestampSeconds - mLastPoll < mInterval) return false;
		mLastPoll = timestampSeconds;
		return true;
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
			model.emptyState = "No active pipeline generation.";
			return model;
		}
		auto compiled = input.graph->compile();
		if (!compiled.valid)
		{
			model.diagnostics = compiled.diagnostics;
			if (model.diagnostics.empty()) model.diagnostics.push_back("Render-graph dependency compilation failed.");
			return model;
		}
		if (input.snapshot)
		{
			auto validPass = [&](GraphPassHandle pass) { return pass.isValid() && pass.id < input.graph->getPassCount(); };
			for (auto pass : input.snapshot->actualPassOrder)
				if (!validPass(pass)) throw std::runtime_error("Flow snapshot contains an invalid compiled pass reference.");
			for (auto const& batch : input.snapshot->batches)
				if (!validPass(batch.parentPass)) throw std::runtime_error("Flow snapshot contains an invalid batch parent pass.");
			for (auto const& event : input.snapshot->physicalEvents)
			{
				if ((event.kind == RenderFlowEventKind::PassBegin || event.kind == RenderFlowEventKind::PassEnd ||
				     event.kind == RenderFlowEventKind::BatchSubmission) && !validPass(event.pass))
					throw std::runtime_error("Flow snapshot contains an invalid event pass reference.");
				if (event.image.isValid() && (event.image.id >= input.graph->getImageCount() ||
				    event.image.version >= input.graph->getImageVersionCount(event.image.id)))
					throw std::runtime_error("Flow snapshot contains an invalid image/version reference.");
			}
		}
		model.pipelineGeneration = input.snapshot ? input.snapshot->pipelineGeneration : 0;
		model.frameSerial = input.snapshot ? input.snapshot->frameSerial : 0;
		model.sceneGeneration = input.sceneGeneration;
		model.liveSample = !!input.snapshot;
		model.stale = input.stale;
		if (input.stale) model.warningBanner = input.staleReason.empty()
		                                           ? "Showing process flow from the last valid pipeline generation."
		                                           : input.staleReason;
		auto stableId = [&](std::string const& key) { return hashText(key, hashText(std::to_string(model.pipelineGeneration))); };
		std::unordered_map<uint64_t, size_t> nodeIndices;
		std::unordered_set<uint64_t> edgeIds;
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
			if (!edgeIds.insert(id).second) return;
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
			for (size_t eventIndex = 0; eventIndex < input.snapshot->physicalEvents.size(); ++eventIndex)
			{
				auto const& event = input.snapshot->physicalEvents[eventIndex];
				if (event.kind == RenderFlowEventKind::PassBegin)
				{
					if (event.pass.id < passNodes.size()) { auto id = passNodes[event.pass.id]; model.findNode(id)->mainSpine = true; model.findNode(id)->sequence = event.sequence; spine.push_back(id); }
					continue;
				}
				if (event.kind == RenderFlowEventKind::PassEnd) continue;
				if (event.kind == RenderFlowEventKind::BatchSubmission)
				{
					std::vector<std::vector<RenderBatchSubmission const*>> materialGroups;
					std::unordered_map<std::string, size_t> materialGroupIndices;
					size_t scan = eventIndex;
					while (scan < input.snapshot->physicalEvents.size() &&
					       input.snapshot->physicalEvents[scan].kind == RenderFlowEventKind::BatchSubmission)
					{
						auto batch = batches.find(input.snapshot->physicalEvents[scan].sequence);
						if (batch == batches.end()) throw std::runtime_error("Flow event references a missing batch submission.");
						auto [group, inserted] = materialGroupIndices.emplace(batch->second->materialName, materialGroups.size());
						if (inserted) materialGroups.emplace_back();
						materialGroups[group->second].push_back(batch->second); ++scan;
					}
					eventIndex = scan - 1;
					for (auto const& group : materialGroups)
					{
						auto const& first = *group.front();
						ProcessFlowNode node;
						node.semanticKey = "batch-group:" + std::to_string(first.sequence) + ":" + std::to_string(group.size());
						node.title = group.size() > 1 ? "Material batch group" :
						             (first.meshName.empty() ? "Submitted batch" : first.meshName);
						node.subtitle = first.materialName.empty() ? first.programName : first.materialName;
						node.kind = group.size() > 1 ? ProcessFlowNodeKind::BatchGroup : ProcessFlowNodeKind::BatchSubmission;
						node.sequence = first.sequence;
						node.submissionCount = group.size();
						node.passId = (int)first.parentPass.id;
						node.materialName = first.materialName;
						uint64_t primitives = 0, instances = 0; bool unresolvedSource = false;
						for (auto const* batch : group)
						{
							primitives += batch->count; instances += batch->instanceCount;
							if (!batch->sceneObject) continue;
							auto object = input.sceneObjects.find(batch->sceneObject);
							if (object == input.sceneObjects.end()) { unresolvedSource = true; continue; }
							if (std::find(node.sceneObjectNames.begin(), node.sceneObjectNames.end(), object->second.name) == node.sceneObjectNames.end())
							{
								node.sceneObjectIndices.push_back(object->second.index);
								node.sceneObjectNames.push_back(object->second.name);
							}
						}
						node.details = std::to_string(group.size()) + " submission(s), " + std::to_string(primitives) +
						               " primitives, " + std::to_string(instances) + " instance(s)";
						if (unresolvedSource) node.details += " | source unavailable for this scene generation";
						node.mainSpine = true;
						node.layoutRank = (float)first.sequence;
						spine.push_back(addNode(std::move(node)));
					}
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
		{
			for (auto pass : actual) { model.findNode(passNodes[pass.id])->mainSpine = true; spine.push_back(passNodes[pass.id]); }
			for (size_t output = 0; output < input.outputPlans.size(); ++output)
			{
				auto const& plan = input.outputPlans[output];
				auto stage = [&](char const* key, char const* title, ProcessFlowNodeKind kind, bool enabled,
				                 char const* reason = "Disabled by effective output setting")
				{
					ProcessFlowNode node; node.semanticKey = "static-output:" + plan.name + ":" + key;
					node.title = plan.name + " / " + title; node.subtitle = "Static output plan; waiting for live sample";
					node.kind = kind; node.enabled = enabled; node.mainSpine = enabled;
					node.bypassReason = enabled ? std::string() : reason; node.layoutRank = 10000.0f + (float)output * 10.0f + (float)spine.size();
					auto id = addNode(std::move(node)); if (enabled) spine.push_back(id); return id;
				};
				stage("msaa", "MSAA resolves", ProcessFlowNodeKind::MsaaResolve,
				      plan.antiAliasing.msaa != AntiAliasingSamples::Off);
				stage("taa", "TAA", ProcessFlowNodeKind::Taa, plan.antiAliasing.taa);
				stage("ssaa-h", "SSAA Horizontal", ProcessFlowNodeKind::Ssaa,
				      plan.antiAliasing.ssaa != AntiAliasingSamples::Off);
				stage("ssaa-v", "SSAA Vertical", ProcessFlowNodeKind::Ssaa,
				      plan.antiAliasing.ssaa != AntiAliasingSamples::Off);
				stage("fxaa", "FXAA", ProcessFlowNodeKind::Fxaa, plan.antiAliasing.fxaa);
				auto presentation = stage("presentation", "Presentation", ProcessFlowNodeKind::Presentation, true);
				if (input.filters.visible(ProcessFlowResourceCategory::NamedOutputs))
				{
					ProcessFlowNode node; node.semanticKey = "output:" + plan.name; node.title = plan.name;
					node.subtitle = std::to_string(plan.logicalSize.x) + " x " + std::to_string(plan.logicalSize.y) + " named output";
					node.kind = ProcessFlowNodeKind::NamedOutput; node.resourceCategory = ProcessFlowResourceCategory::NamedOutputs;
					node.layoutRank = 11000.0f + (float)output; auto outputId = addNode(std::move(node));
					if (input.filters.resourceEdges) addEdge(presentation, outputId, ProcessFlowEdgeKind::Output, plan.name);
				}
			}
		}
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
						node.details = info.desc.absoluteSize.x && info.desc.absoluteSize.y
						                   ? std::to_string(info.desc.absoluteSize.x) + " x " + std::to_string(info.desc.absoluteSize.y)
						                   : std::to_string((int)(info.desc.relativeSize.x * 100)) + "% x " +
						                         std::to_string((int)(info.desc.relativeSize.y * 100)) + "%";
						node.details += ", " + std::to_string(info.desc.mipLevels) + " mip(s), " +
						                (info.desc.transient ? "transient" : "persistent") +
						                (info.desc.external ? ", external" : "");
						node.kind = imported ? ProcessFlowNodeKind::Import : ProcessFlowNodeKind::AuthoredImage;
						node.resourceCategory = category;
						node.imageId = (int)image.id;
						if (imported)
						{
							auto import = input.imports.find(info.importName);
							if (import != input.imports.end()) node.importIndex = import->second;
						}
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
		ProcessFlowSampleGate sampleGate;
		if (!sampleGate.poll(1.0) || sampleGate.poll(1.1) || !sampleGate.poll(1.25) || !sampleGate.poll(1.26, true))
			throw std::runtime_error("Process-flow sampling interval/forced refresh test failed.");
		RenderGraph graph;
		GraphImageDesc desc; desc.usage = GraphImageUsage::ColourAttachment | GraphImageUsage::Sampled;
		auto image = graph.createImage("FlowColour", desc);
		auto producer = graph.addPass("Producer"); auto produced = graph.writeColour(producer, image);
		auto consumer = graph.addPass("Consumer"); graph.readSampled(consumer, produced);
		auto snapshot = std::make_shared<RenderPipelineFlowSnapshot>(); snapshot->pipelineGeneration = 9;
		snapshot->actualPassOrder = {consumer, producer};
		int sourceIdentity = 0;
		RenderBatchSubmission first; first.sequence = 2; first.parentPass = consumer; first.meshName = "Duplicate"; first.materialName = "SameMaterial";
		first.sceneObject = reinterpret_cast<SceneModel3d const*>(&sourceIdentity);
		auto second = first; second.sequence = 3; second.materialName = "OtherMaterial";
		auto third = first; third.sequence = 4; snapshot->batches = {first, second, third};
		snapshot->physicalEvents = {{RenderFlowEventKind::PassBegin, 1, consumer}, {RenderFlowEventKind::BatchSubmission, 2, consumer}, {RenderFlowEventKind::BatchSubmission, 3, consumer}, {RenderFlowEventKind::BatchSubmission, 4, consumer}, {RenderFlowEventKind::PassEnd, 5, consumer}, {RenderFlowEventKind::PassBegin, 6, producer}, {RenderFlowEventKind::PassEnd, 7, producer}};
		ProcessFlowModelBuilder builder;
		if (builder.build({}).emptyState != "No active pipeline generation.")
			throw std::runtime_error("Process-flow no-generation state test failed.");
		ProcessFlowBuildInput input{&graph, snapshot};
		input.sceneGeneration = 12;
		input.sceneObjects[first.sceneObject] = {4, "FlowObject"};
		auto model = builder.build(input);
		if (std::count_if(model.nodes.begin(), model.nodes.end(), [](auto const& node)
		    { return node.kind == ProcessFlowNodeKind::BatchGroup && node.submissionCount == 2; }) != 1)
			throw std::runtime_error("Process-flow model did not group same-material submissions.");
		if (!model.findNode(model.nodes[0].id) || !std::any_of(model.nodes.begin(), model.nodes.end(), [](auto const& node) { return node.orderWarning; }))
			throw std::runtime_error("Process-flow model stable identity/order warning test failed.");
		if (model.sceneGeneration != 12 || std::none_of(model.nodes.begin(), model.nodes.end(), [](auto const& node)
		    { return node.kind == ProcessFlowNodeKind::BatchGroup && node.sceneObjectNames == std::vector<std::string>{"FlowObject"} && node.sceneObjectIndices == std::vector<int>{4}; }))
			throw std::runtime_error("Process-flow generation-safe scene-object resolution test failed.");
		auto repeated = builder.build(input);
		input.stale = true; input.staleReason = "Last valid generation";
		auto stale = builder.build(input);
		if (!stale.stale || stale.warningBanner != input.staleReason) throw std::runtime_error("Process-flow stale state test failed.");
		input.stale = false; input.staleReason.clear();
		for (auto const& node : model.nodes)
		{
			auto match = std::find_if(repeated.nodes.begin(), repeated.nodes.end(), [&](auto const& value) { return value.semanticKey == node.semanticKey; });
			if (match == repeated.nodes.end() || match->id != node.id) throw std::runtime_error("Process-flow IDs changed within one generation.");
		}
		snapshot->pipelineGeneration = 10;
		auto regenerated = builder.build(input);
		if (regenerated.nodes.front().id == model.nodes.front().id) throw std::runtime_error("Process-flow IDs survived a pipeline generation change.");
		snapshot->pipelineGeneration = 9;
		auto validOrder = snapshot->actualPassOrder;
		snapshot->actualPassOrder.push_back({9999});
		try { (void)builder.build(input); throw std::runtime_error("Process-flow malformed snapshot was accepted."); }
		catch (std::runtime_error const& error)
		{
			if (std::string(error.what()) == "Process-flow malformed snapshot was accepted.") throw;
		}
		snapshot->actualPassOrder = std::move(validOrder);
		auto staticInput = input; staticInput.snapshot.reset();
		RenderPipelineOutputPlan staticPlan; staticPlan.name = "Main"; staticPlan.logicalSize = {640, 360};
		staticInput.outputPlans.push_back(staticPlan);
		auto waiting = builder.build(staticInput);
		if (waiting.liveSample || std::none_of(waiting.nodes.begin(), waiting.nodes.end(), [](auto const& node)
		    { return node.kind == ProcessFlowNodeKind::Presentation; }) ||
		    std::none_of(waiting.nodes.begin(), waiting.nodes.end(), [](auto const& node)
		    { return node.kind == ProcessFlowNodeKind::Taa && !node.enabled && !node.bypassReason.empty(); }))
			throw std::runtime_error("Process-flow static waiting/output-stage test failed.");
		input.filters.resources = (uint32_t)ProcessFlowResourceCategory::AuthoredImages;
		auto resources = builder.build(input);
		if (std::none_of(resources.nodes.begin(), resources.nodes.end(), [](auto const& node) { return node.kind == ProcessFlowNodeKind::AuthoredImage; }))
			throw std::runtime_error("Process-flow authored-resource transformation test failed.");
	}
}
