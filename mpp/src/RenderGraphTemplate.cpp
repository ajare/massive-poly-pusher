#include <glew/glew.h>

#include "mpp/MppException.h"
#include "mpp/RenderGraphStream.h"
#include "mpp/RenderGraphTemplate.h"
#include "mpp/ResourceManager.h"

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
		mPrograms.clear();
		for (uint32_t id = 0; id < mGraph->getPassCount(); ++id)
		{
			GraphPassHandle pass{ id };
			auto const info = mGraph->getPassInfo(pass);
			if (info.programResource.empty()) continue;
			auto program = getResourceManager()->getResource(info.programResource, true);
			if (!program)
			{
				THROW_MPP("RenderGraph pass '" + info.name + "' references missing program '" + info.programResource + "'.", __LINE__, __FILE__, __func__);
			}
			if (program->getType() != "Program")
			{
				THROW_MPP("RenderGraph pass '" + info.name + "' reference '" + info.programResource + "' is not a Program.", __LINE__, __FILE__, __func__);
			}
			program->create();
			program->load();
			mPrograms[id] = program;
		}
	}

	void RenderGraphTemplate::destroyImpl() { mPrograms.clear(); mGraph.reset(); }
	void RenderGraphTemplate::loadImpl() {}
	void RenderGraphTemplate::unloadImpl() {}

	std::shared_ptr<RenderGraph> const& RenderGraphTemplate::getGraph() const { return mGraph; }

	ResourcePtr RenderGraphTemplate::getProgram(GraphPassHandle pass) const
	{
		auto const found = mPrograms.find(pass.id);
		return found == mPrograms.end() ? nullptr : found->second;
	}
}
