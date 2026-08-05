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

		float parameter(RenderGraphExecutionContext const& context, std::string const& name, float fallback)
		{
			auto const& values = context.getParameters().getUniformData();
			auto const found = values.find(name);
			return found == values.end() || found->second.size < sizeof(float) ? fallback : *reinterpret_cast<float const*>(found->second.data);
		}

		Texture* input(RenderGraphExecutionContext const& context, size_t index)
		{
			auto const& bindings = context.getPass().samplerBindings;
			return index < bindings.size() ? dynamic_cast<Texture*>(context.getImage(bindings[index].image).get()) : nullptr;
		}

		class BloomExtractPass final : public RenderGraphScenePass
		{
		public:
			void execute(RenderGraphExecutionContext const& context) override { context.getFrame().renderSystem->renderBloomExtract(input(context, 0), parameter(context, "THRESHOLD", 1.0f)); }
		};
		class BloomBlurPass final : public RenderGraphScenePass
		{
			bool mHorizontal;
		public:
			explicit BloomBlurPass(bool horizontal) : mHorizontal(horizontal) {}
			void execute(RenderGraphExecutionContext const& context) override { context.getFrame().renderSystem->renderBloomBlur(input(context, 0), mHorizontal ? glm::vec2(1, 0) : glm::vec2(0, 1)); }
		};
		class BloomCompositePass final : public RenderGraphScenePass
		{
		public:
			void execute(RenderGraphExecutionContext const& context) override { context.getFrame().renderSystem->renderBloomCombine(input(context, 0), input(context, 1), parameter(context, "INTENSITY", 0.15f)); }
		};
		class ToneMapPresentPass final : public RenderGraphScenePass
		{
		public:
			void execute(RenderGraphExecutionContext const& context) override { context.getFrame().renderSystem->renderToneMappedFullscreenQuad(input(context, 0), parameter(context, "EXPOSURE", 1.0f), parameter(context, "TONE_MAP_OPERATOR", 1.0f) != 0.0f); }
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
		registry.registerScenePassFactory("MPP.BloomExtract", [] { return std::make_unique<BloomExtractPass>(); });
		registry.registerScenePassFactory("MPP.BloomBlurHorizontal", [] { return std::make_unique<BloomBlurPass>(true); });
		registry.registerScenePassFactory("MPP.BloomBlurVertical", [] { return std::make_unique<BloomBlurPass>(false); });
		registry.registerScenePassFactory("MPP.BloomComposite", [] { return std::make_unique<BloomCompositePass>(); });
		registry.registerScenePassFactory("MPP.ToneMapPresent", [] { return std::make_unique<ToneMapPresentPass>(); });
		registry.registerScenePassFactory("MPP.PbrScene", [] { return std::make_unique<ScenePass>(); });
		registry.registerScenePassFactory("MPP.LegacyScene", [] { return std::make_unique<ScenePass>(); });
	}
}
