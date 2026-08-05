#include "mpp/MppException.h"
#include "mpp/RenderGraphStream.h"
#include "mpp/RenderGraphTemplate.h"

namespace mpp
{
	RenderGraphTemplate::RenderGraphTemplate(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream)
		: Resource(name, "RenderGraph", renderSystem, resourceMgr, resourceStream)
	{
	}

	void RenderGraphTemplate::createImpl()
	{
		auto stream = dynamic_cast<RenderGraphStream*>(getResourceStream().get());
		if (!stream || !stream->getGraph())
		{
			THROW_MPP("RenderGraph resource requires a populated RenderGraphStream.", __LINE__, __FILE__, __func__);
		}
		mGraph = stream->getGraph();
	}

	void RenderGraphTemplate::destroyImpl() { mGraph.reset(); }
	void RenderGraphTemplate::loadImpl() {}
	void RenderGraphTemplate::unloadImpl() {}

	std::shared_ptr<RenderGraph> const& RenderGraphTemplate::getGraph() const { return mGraph; }
}
