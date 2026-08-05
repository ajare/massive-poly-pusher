#include "mpp/RenderGraphBuiltInPasses.h"
#include "mpp/RenderGraphExecutor.h"
#include "mpp/RenderGraphPassFactoryRegistry.h"
#include "mpp/RenderGraphScenePass.h"
#include "mpp/RenderSystem.h"
#include "mpp/RenderPipeline.h"

namespace mpp
{
	namespace
	{
		class ShadowDepthPass final : public RenderGraphScenePass
		{
		public:
			void execute(RenderGraphExecutionContext const& context) override
			{
				auto const& frame = context.getFrame();
				if (frame.pipelineOptions && !frame.pipelineOptions->shadowDomain.empty())
					frame.renderSystem->renderShadowDomain(frame.pipelineOptions->shadowDomain, frame.visibleModels);
			}
		};

		class ScenePass final : public RenderGraphScenePass
		{
		public:
			void execute(RenderGraphExecutionContext const& context) override
			{
				auto const& frame = context.getFrame();
				if (frame.scene && frame.scene->show3dModels() && frame.sceneRenderPass && !frame.visibleModels.empty())
				{
					frame.sceneRenderPass->render(frame.visibleModels, frame.camera);
					frame.renderSystem->flushVertexBuffers();
				}
			}
		};
	}

	void registerBuiltInRenderGraphPasses(RenderGraphPassFactoryRegistry& registry)
	{
		registry.registerScenePassFactory("MPP.ShadowDepth", [] { return std::make_unique<ShadowDepthPass>(); });
		registry.registerScenePassFactory("MPP.PbrScene", [] { return std::make_unique<ScenePass>(); });
		registry.registerScenePassFactory("MPP.LegacyScene", [] { return std::make_unique<ScenePass>(); });
	}
}
