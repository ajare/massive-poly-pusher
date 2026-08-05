#include <algorithm>
#include <queue>
#include <set>
#include <sstream>

#include "mpp/RenderGraph.h"
#include "mpp/Caps.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{
	namespace
	{
		bool isDepthFormat(GraphImageFormat format)
		{
			return format == GraphImageFormat::Depth24 || format == GraphImageFormat::Depth24Stencil8;
		}
	}

	struct RenderGraph::Image
	{
		string name;
		GraphImageDesc desc;
		uint32_t latestVersion{ 0 };
		// Producer pass per version. UINT32_MAX means imported/external version 0.
		vector<uint32_t> producers{ UINT32_MAX };
	};

	struct RenderGraph::Pass
	{
		string name;
		string callbackFactory;
		string programResource;
		vector<GraphImageHandle> sampledInputs;
		vector<GraphSamplerBinding> samplerBindings;
		vector<GraphColourOutput> colourOutputs;
		vector<GraphDepthOutput> depthOutputs;
	};

	RenderGraph::RenderGraph() = default;
	RenderGraph::~RenderGraph() = default;
	RenderGraph::RenderGraph(RenderGraph&&) noexcept = default;
	RenderGraph& RenderGraph::operator =(RenderGraph&&) noexcept = default;

	bool RenderGraph::validImage(GraphImageHandle image) const
	{
		return image.isValid() && image.id < mImages.size() && image.version < mImages[image.id].producers.size();
	}

	bool RenderGraph::validPass(GraphPassHandle pass) const
	{
		return pass.isValid() && pass.id < mPasses.size();
	}

	GraphImageHandle RenderGraph::createImage(string const& name, GraphImageDesc const& desc)
	{
		bool const depthFormat = isDepthFormat(desc.format);
		bool const colourUsage = hasGraphImageUsage(desc.usage, GraphImageUsage::ColourAttachment);
		bool const depthUsage = hasGraphImageUsage(desc.usage, GraphImageUsage::DepthAttachment);
		if (name.empty() || desc.samples == 0 || desc.mipLevels == 0 ||
			(desc.absoluteSize.x == 0 && desc.relativeSize.x <= 0.0f) ||
			(desc.absoluteSize.y == 0 && desc.relativeSize.y <= 0.0f) ||
			(depthFormat && (!depthUsage || colourUsage)) || (!depthFormat && depthUsage))
		{
			THROW_MPP("Invalid render graph image descriptor.", __LINE__, __FILE__, __func__);
		}
		if (find_if(mImages.begin(), mImages.end(), [&](Image const& image) { return image.name == name; }) != mImages.end())
		{
			THROW_MPP("Duplicate render graph image name.", __LINE__, __FILE__, __func__);
		}
		mImages.push_back({ name, desc });
		return { (uint32_t)mImages.size() - 1, 0 };
	}

	GraphPassHandle RenderGraph::addPass(string const& name)
	{
		if (name.empty() || find_if(mPasses.begin(), mPasses.end(), [&](Pass const& pass) { return pass.name == name; }) != mPasses.end())
		{
			THROW_MPP("Invalid or duplicate render graph pass name.", __LINE__, __FILE__, __func__);
		}
		mPasses.push_back({ name });
		return { (uint32_t)mPasses.size() - 1 };
	}

	void RenderGraph::setPassProgramResource(GraphPassHandle pass, string const& resourceName)
	{
		if (!validPass(pass) || resourceName.empty())
		{
			THROW_MPP("Render graph pass program resource requires a valid pass and name.", __LINE__, __FILE__, __func__);
		}
		mPasses[pass.id].programResource = resourceName;
	}

	void RenderGraph::setPassCallbackFactory(GraphPassHandle pass, string const& factoryName)
	{
		if (!validPass(pass) || factoryName.empty())
		{
			THROW_MPP("Render graph pass callback factory requires a valid pass and name.", __LINE__, __FILE__, __func__);
		}
		mPasses[pass.id].callbackFactory = factoryName;
	}

	void RenderGraph::readSampled(GraphPassHandle pass, GraphImageHandle image)
	{
		if (!validPass(pass) || !validImage(image))
		{
			THROW_MPP("Invalid render graph sampled input.", __LINE__, __FILE__, __func__);
		}
		mPasses[pass.id].sampledInputs.push_back(image);
	}

	void RenderGraph::bindSampler(GraphPassHandle pass, string const& sampler, GraphImageHandle image)
	{
		if (!validPass(pass) || !validImage(image) || sampler.empty())
		{
			THROW_MPP("Invalid render graph sampler binding.", __LINE__, __FILE__, __func__);
		}
		auto& bindings = mPasses[pass.id].samplerBindings;
		if (find_if(bindings.begin(), bindings.end(), [&](GraphSamplerBinding const& binding) { return binding.sampler == sampler; }) != bindings.end())
		{
			THROW_MPP("Duplicate render graph sampler binding.", __LINE__, __FILE__, __func__);
		}
		readSampled(pass, image);
		bindings.push_back({ sampler, image });
	}

	GraphImageHandle RenderGraph::writeColour(GraphPassHandle pass, GraphImageHandle image, GraphLoadOp load, GraphStoreOp store, glm::vec4 const& clear)
	{
		if (!validPass(pass) || !validImage(image) || image.version != mImages[image.id].latestVersion ||
			!hasGraphImageUsage(mImages[image.id].desc.usage, GraphImageUsage::ColourAttachment) || isDepthFormat(mImages[image.id].desc.format))
		{
			THROW_MPP("Invalid render graph colour output.", __LINE__, __FILE__, __func__);
		}
		auto& target = mImages[image.id];
		GraphImageHandle output{ image.id, ++target.latestVersion };
		target.producers.push_back(pass.id);
		mPasses[pass.id].colourOutputs.push_back({ output, load, store, clear });
		return output;
	}

	GraphImageHandle RenderGraph::writeDepth(GraphPassHandle pass, GraphImageHandle image, GraphLoadOp load, GraphStoreOp store, float clear)
	{
		if (!validPass(pass) || !validImage(image) || image.version != mImages[image.id].latestVersion ||
			!hasGraphImageUsage(mImages[image.id].desc.usage, GraphImageUsage::DepthAttachment) || !isDepthFormat(mImages[image.id].desc.format))
		{
			THROW_MPP("Invalid render graph depth output.", __LINE__, __FILE__, __func__);
		}
		auto& target = mImages[image.id];
		GraphImageHandle output{ image.id, ++target.latestVersion };
		target.producers.push_back(pass.id);
		mPasses[pass.id].depthOutputs.push_back({ output, load, store, clear });
		return output;
	}

	GraphPassInfo RenderGraph::getPassInfo(GraphPassHandle pass) const
	{
		if (!validPass(pass))
		{
			THROW_MPP("Invalid render graph pass handle.", __LINE__, __FILE__, __func__);
		}
		auto const& source = mPasses[pass.id];
		return { source.name, source.callbackFactory, source.programResource, source.sampledInputs, source.samplerBindings, source.colourOutputs, source.depthOutputs };
	}

	RenderGraphCompileResult RenderGraph::compile() const
	{
		RenderGraphCompileResult result;
		vector<set<uint32_t>> edges(mPasses.size());
		vector<uint32_t> indegree(mPasses.size());

		for (uint32_t passId = 0; passId < mPasses.size(); ++passId)
		{
			auto const& pass = mPasses[passId];
			for (auto const& input : pass.sampledInputs)
			{
				if (!validImage(input))
				{
					result.diagnostics.push_back("Pass '" + pass.name + "' reads an invalid image handle.");
					continue;
				}
				auto const& image = mImages[input.id];
				if (!hasGraphImageUsage(image.desc.usage, GraphImageUsage::Sampled))
				{
					result.diagnostics.push_back("Pass '" + pass.name + "' samples image '" + image.name + "' without Sampled usage.");
				}
				uint32_t producer = image.producers[input.version];
				if (producer == UINT32_MAX && !image.desc.external)
				{
					result.diagnostics.push_back("Pass '" + pass.name + "' reads unwritten non-external image '" + image.name + "'.");
				}
				else if (producer != UINT32_MAX && producer != passId)
				{
					edges[producer].insert(passId);
				}
				else if (producer == passId)
				{
					result.diagnostics.push_back("Pass '" + pass.name + "' reads and writes the same image version.");
				}
			}

			if (pass.colourOutputs.size() > 1)
			{
				auto const& first = mImages[pass.colourOutputs.front().image.id].desc;
				for (auto const& output : pass.colourOutputs)
				{
					auto const& desc = mImages[output.image.id].desc;
					if (desc.absoluteSize != first.absoluteSize || desc.relativeSize != first.relativeSize || desc.samples != first.samples)
					{
						result.diagnostics.push_back("MRT pass '" + pass.name + "' has incompatible colour attachment dimensions or sample counts.");
					}
				}
			}
			if (pass.depthOutputs.size() > 1)
			{
				result.diagnostics.push_back("Pass '" + pass.name + "' declares more than one depth output.");
			}
			if (!pass.colourOutputs.empty() && !pass.depthOutputs.empty())
			{
				auto const& colour = mImages[pass.colourOutputs.front().image.id].desc;
				auto const& depth = mImages[pass.depthOutputs.front().image.id].desc;
				if (colour.absoluteSize != depth.absoluteSize || colour.relativeSize != depth.relativeSize || colour.samples != depth.samples)
				{
					result.diagnostics.push_back("Pass '" + pass.name + "' has incompatible colour and depth attachment dimensions or sample counts.");
				}
			}
		}

		if (!result.diagnostics.empty()) return result;
		for (uint32_t source = 0; source < edges.size(); ++source)
			for (uint32_t destination : edges[source]) ++indegree[destination];
		queue<uint32_t> ready;
		for (uint32_t i = 0; i < indegree.size(); ++i) if (indegree[i] == 0) ready.push(i);
		while (!ready.empty())
		{
			uint32_t pass = ready.front(); ready.pop();
			result.passOrder.push_back({ pass });
			for (uint32_t next : edges[pass]) if (--indegree[next] == 0) ready.push(next);
		}
		if (result.passOrder.size() != mPasses.size())
		{
			result.diagnostics.push_back("Render graph contains a pass dependency cycle.");
			result.passOrder.clear();
			return result;
		}
		result.valid = true;
		return result;
	}

	RenderGraphAllocationPlan RenderGraph::buildAllocationPlan(glm::uvec2 const& viewport) const
	{
		RenderGraphAllocationPlan plan;
		if (viewport.x == 0 || viewport.y == 0)
		{
			plan.diagnostics.push_back("Render graph allocation viewport must be non-zero.");
			return plan;
		}
		auto compiled = compile();
		if (!compiled.valid)
		{
			plan.diagnostics = move(compiled.diagnostics);
			return plan;
		}

		vector<vector<uint32_t>> allocationIndex;
		allocationIndex.reserve(mImages.size());
		for (auto const& image : mImages)
			allocationIndex.emplace_back(image.producers.size(), UINT32_MAX);

		auto markImage = [&](GraphImageHandle handle, uint32_t passPosition)
		{
			auto const& image = mImages[handle.id];
			if (image.desc.external)
			{
				if (find_if(plan.importedImages.begin(), plan.importedImages.end(), [&](GraphImageHandle current)
					{ return current.id == handle.id && current.version == handle.version; }) == plan.importedImages.end())
				{
					plan.importedImages.push_back(handle);
				}
				return;
			}
			auto& index = allocationIndex[handle.id][handle.version];
			if (index == UINT32_MAX)
			{
				GraphImageLifetime lifetime;
				lifetime.image = handle;
				lifetime.desc = image.desc;
				lifetime.size.x = image.desc.absoluteSize.x ? image.desc.absoluteSize.x : max(1u, (uint32_t)(viewport.x * image.desc.relativeSize.x));
				lifetime.size.y = image.desc.absoluteSize.y ? image.desc.absoluteSize.y : max(1u, (uint32_t)(viewport.y * image.desc.relativeSize.y));
				lifetime.firstPass = passPosition;
				lifetime.lastPass = passPosition;
				index = (uint32_t)plan.allocatedImages.size();
				plan.allocatedImages.push_back(lifetime);
			}
			else
			{
				plan.allocatedImages[index].firstPass = min(plan.allocatedImages[index].firstPass, passPosition);
				plan.allocatedImages[index].lastPass = max(plan.allocatedImages[index].lastPass, passPosition);
			}
		};

		for (uint32_t position = 0; position < compiled.passOrder.size(); ++position)
		{
			auto const& pass = mPasses[compiled.passOrder[position].id];
			for (auto const& input : pass.sampledInputs) markImage(input, position);
			for (auto const& output : pass.colourOutputs) markImage(output.image, position);
			for (auto const& output : pass.depthOutputs) markImage(output.image, position);
		}
		plan.valid = true;
		return plan;
	}

	string RenderGraph::describe() const
	{
		ostringstream output;
		output << "RenderGraph: " << mImages.size() << " image(s), " << mPasses.size() << " pass(es)\n";
		for (uint32_t id = 0; id < mImages.size(); ++id)
		{
			auto const& image = mImages[id];
			output << "  Image[" << id << "] '" << image.name << "': versions=0.." << image.latestVersion
				<< ", size=" << image.desc.absoluteSize.x << "x" << image.desc.absoluteSize.y
				<< " @ " << image.desc.relativeSize.x << "x" << image.desc.relativeSize.y
				<< ", samples=" << image.desc.samples << ", transient=" << (image.desc.transient ? "true" : "false")
				<< ", external=" << (image.desc.external ? "true" : "false") << "\n";
		}
		for (uint32_t id = 0; id < mPasses.size(); ++id)
		{
			auto const& pass = mPasses[id];
			output << "  Pass[" << id << "] '" << pass.name << "': sampled=" << pass.sampledInputs.size()
				<< ", colour=" << pass.colourOutputs.size() << ", depth=" << pass.depthOutputs.size() << "\n";
			for (auto const& input : pass.sampledInputs)
				output << "    reads Image[" << input.id << "]@" << input.version << "\n";
			for (auto const& target : pass.colourOutputs)
				output << "    writes colour Image[" << target.image.id << "]@" << target.image.version << "\n";
			for (auto const& target : pass.depthOutputs)
				output << "    writes depth Image[" << target.image.id << "]@" << target.image.version << "\n";
		}
		return output.str();
	}

	RenderGraphCompileResult RenderGraph::compile(Caps const& caps) const
	{
		auto result = compile();
		for (auto const& pass : mPasses)
		{
			if (pass.colourOutputs.size() > caps.maxColourAttachments || pass.colourOutputs.size() > caps.maxDrawBuffers)
			{
				result.diagnostics.push_back("Pass '" + pass.name + "' declares " + to_string(pass.colourOutputs.size()) +
					" colour outputs, exceeding the renderer MRT capability.");
			}
		}
		if (!result.diagnostics.empty())
		{
			result.valid = false;
			result.passOrder.clear();
		}
		return result;
	}
}
