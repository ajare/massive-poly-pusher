#pragma once

#include <functional>
#include <map>
#include <memory>

#include "mpp/Config.h"
#include "mpp/RenderGraph.h"
#include "mpp/RenderGraphTargets.h"

namespace mpp
{
	class RenderSystem;
	class RenderGraphPassFactoryRegistry;
	class RenderGraphScenePass;

	class _MPPAPI RenderGraphExecutionContext
	{
		RenderGraphTargets const* mTargets;
		UniformCollection const* mParameters;

	public:
		explicit RenderGraphExecutionContext(RenderGraphTargets const* targets, UniformCollection const* parameters = nullptr);
		RenderTargetPtr getImage(GraphImageHandle image) const;
		UniformCollection const& getParameters() const;
	};

	// Executes graphics passes supplied by application callbacks. It creates a
	// temporary framebuffer view for each pass; graph textures remain owned by
	// RenderGraphTargets.
	class _MPPAPI RenderGraphExecutor
	{
		RenderSystem* mRenderSystem;
		RenderGraphPassFactoryRegistry const* mFactoryRegistry{ nullptr };
		std::map<uint32_t, std::function<void(RenderGraphExecutionContext const&)>> mCallbacks;
		std::map<uint32_t, std::unique_ptr<RenderGraphScenePass>> mScenePasses;
		std::map<uint32_t, UniformCollection> mParameterOverrides;

	public:
		explicit RenderGraphExecutor(RenderSystem* renderSystem);
		~RenderGraphExecutor();
		RenderGraphExecutor(RenderGraphExecutor const&) = delete;
		RenderGraphExecutor& operator =(RenderGraphExecutor const&) = delete;

		void setPassCallback(GraphPassHandle pass, std::function<void(RenderGraphExecutionContext const&)> callback);
		void setPassFactoryRegistry(RenderGraphPassFactoryRegistry const* registry);
		void setPassParameterOverrides(GraphPassHandle pass, UniformCollection const& parameters);
		void clearPassCallbacks();
		void execute(RenderGraph const& graph, RenderGraphTargets const& targets, Caps const& caps);
	};
}
