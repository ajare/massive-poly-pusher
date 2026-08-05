#pragma once

#include <functional>
#include <map>

#include "mpp/Config.h"
#include "mpp/RenderGraph.h"
#include "mpp/RenderGraphTargets.h"

namespace mpp
{
	class RenderSystem;

	class _MPPAPI RenderGraphExecutionContext
	{
		RenderGraphTargets const* mTargets;

	public:
		explicit RenderGraphExecutionContext(RenderGraphTargets const* targets);
		RenderTargetPtr getImage(GraphImageHandle image) const;
	};

	// Executes graphics passes supplied by application callbacks. It creates a
	// temporary framebuffer view for each pass; graph textures remain owned by
	// RenderGraphTargets.
	class _MPPAPI RenderGraphExecutor
	{
		RenderSystem* mRenderSystem;
		std::map<uint32_t, std::function<void(RenderGraphExecutionContext const&)>> mCallbacks;

	public:
		explicit RenderGraphExecutor(RenderSystem* renderSystem);

		void setPassCallback(GraphPassHandle pass, std::function<void(RenderGraphExecutionContext const&)> callback);
		void clearPassCallbacks();
		void execute(RenderGraph const& graph, RenderGraphTargets const& targets, Caps const& caps);
	};
}
