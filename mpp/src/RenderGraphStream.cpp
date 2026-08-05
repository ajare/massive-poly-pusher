#include "mpp/MppException.h"
#include "mpp/RenderGraphStream.h"

namespace mpp
{
	RenderGraphStream::RenderGraphStream(ResourceManager* resourceMgr)
		: ResourceStream(resourceMgr, "RenderGraph")
	{
	}

	void RenderGraphStream::setGraph(std::shared_ptr<RenderGraph> graph)
	{
		if (!graph)
		{
			THROW_MPP("Render graph stream requires a graph template.", __LINE__, __FILE__, __func__);
		}
		mGraph = std::move(graph);
	}

	std::shared_ptr<RenderGraph> const& RenderGraphStream::getGraph() const
	{
		return mGraph;
	}

	uint32_t RenderGraphStream::createQualitySetting(std::string const& name)
	{
		auto id = (uint32_t)mQualityNames.size();
		mQualityNames[name] = id;
		return id;
	}
}
