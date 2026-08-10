#include <algorithm>
#include <string>

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
				if (frame.pipelineOptions && frame.pipelineOptions->graphPasses.shadow && !frame.pipelineOptions->shadowDomain.empty())
					frame.renderSystem->renderShadowDomain(frame.pipelineOptions->shadowDomain, frame.visibleModels);
			}
		};

		float parameter(RenderGraphExecutionContext const& context, std::string const& name, float fallback)
		{
			auto const& values = context.getParameters().getUniformData();
			auto const found = values.find(name);
			return found == values.end() || found->second.size < sizeof(float) ? fallback : *reinterpret_cast<float const*>(found->second.data);
		}

		int32_t integerParameter(RenderGraphExecutionContext const& context, std::string const& name, int32_t fallback)
		{
			auto const& values = context.getParameters().getUniformData();
			auto const found = values.find(name);
			return found == values.end() || found->second.size < sizeof(int32_t) ? fallback : *reinterpret_cast<int32_t const*>(found->second.data);
		}

		Texture* input(RenderGraphExecutionContext const& context, size_t index)
		{
			auto const& bindings = context.getPass().samplerBindings;
			return index < bindings.size() ? dynamic_cast<Texture*>(context.getImage(bindings[index].image).get()) : nullptr;
		}
		Texture* input(RenderGraphExecutionContext const& context, std::string const& sampler)
		{
			auto const& bindings = context.getPass().samplerBindings;
			auto found = std::find_if(bindings.begin(), bindings.end(), [&](auto const& binding)
			{
				return binding.sampler == sampler;
			});
			return found != bindings.end() ? dynamic_cast<Texture*>(context.getImage(found->image).get()) : nullptr;
		}

		class BloomExtractPass final : public RenderGraphScenePass
		{
		public:
			void execute(RenderGraphExecutionContext const& context) override { if (context.getFrame().pipelineOptions->bloom.enabled && context.getFrame().pipelineOptions->graphPasses.bloom) context.getFrame().renderSystem->renderBloomExtract(input(context, 0), parameter(context, "THRESHOLD", 1.0f)); }
		};
		// Deprecated fallback for graphs authored before blur passes declared an
		// ITERATION parameter. Renaming such a pass changes how many blur levels the
		// blurPasses option enables, which is why the parameter replaced it.
		uint32_t trailingPassIndex(std::string const& name)
		{
			auto first=name.find_last_not_of("0123456789");if(first==name.size()-1)return 0;try{return (uint32_t)std::stoul(name.substr(first+1));}catch(...){return 0;}
		}

		class BloomBlurPass final : public RenderGraphScenePass
		{
			bool mHorizontal;
		public:
			explicit BloomBlurPass(bool horizontal) : mHorizontal(horizontal) {}
			void execute(RenderGraphExecutionContext const& context) override
			{
				auto const& frame = context.getFrame();
				auto const iteration = bloomBlurIteration(context.getParameters(), context.getPass().name);
				bool enabled = frame.pipelineOptions->bloom.enabled && frame.pipelineOptions->graphPasses.bloom && iteration < frame.pipelineOptions->bloom.blurPasses;
				if (enabled) frame.renderSystem->renderBloomBlur(input(context, 0), mHorizontal ? glm::vec2(1, 0) : glm::vec2(0, 1));
				else frame.renderSystem->renderFullscreenQuad(input(context, 0), BlendMode::One, BlendMode::Zero);
			}
		};
		class BloomCompositePass final : public RenderGraphScenePass
		{
		public:
			void execute(RenderGraphExecutionContext const& context) override
			{
				auto const& frame = context.getFrame();
				if (frame.pipelineOptions->bloom.enabled && frame.pipelineOptions->graphPasses.bloom)
					frame.renderSystem->renderBloomCombine(input(context, "SCENE"), input(context, "BLOOM"), parameter(context, "INTENSITY", 0.15f));
				else
					frame.renderSystem->renderFullscreenQuad(input(context, "SCENE"), BlendMode::One, BlendMode::Zero);
			}
		};
		class ToneMapPresentPass final : public RenderGraphScenePass
		{
		public:
			void execute(RenderGraphExecutionContext const& context) override { if (context.getFrame().pipelineOptions->graphPasses.presentation) context.getFrame().renderSystem->renderToneMappedFullscreenQuad(input(context, 0), parameter(context, "EXPOSURE", 1.0f), integerParameter(context, "TONE_MAP_OPERATOR", 1) != 0); }
		};

		std::vector<GraphImageFormat> colourFormats()
		{
			return { GraphImageFormat::R8, GraphImageFormat::Rg8, GraphImageFormat::Rgba8, GraphImageFormat::Srgb8Alpha8,
				GraphImageFormat::R16f, GraphImageFormat::Rg16f, GraphImageFormat::Rgba16f, GraphImageFormat::R32f,
				GraphImageFormat::Rg32f, GraphImageFormat::Rgba32f, GraphImageFormat::R11g11b10f, GraphImageFormat::Rgb10a2 };
		}
		std::vector<GraphImageFormat> depthFormats()
		{
			return { GraphImageFormat::Depth16, GraphImageFormat::Depth24, GraphImageFormat::Depth32f, GraphImageFormat::Depth24Stencil8, GraphImageFormat::Depth32fStencil8 };
		}
		GraphPassAuthoringMetadata metadata(std::string displayName, std::string category, GraphPassType type)
		{
			GraphPassAuthoringMetadata result;
			result.displayName = std::move(displayName); result.category = std::move(category); result.type = type; result.supportsRasterState = true;
			return result;
		}

		class ScenePass final : public RenderGraphScenePass
		{
		public:
			void execute(RenderGraphExecutionContext const& context) override
			{
				auto const& frame = context.getFrame();
				if (!frame.pipelineOptions->graphPasses.scene) return;
				if (frame.pipelineOptions->debugEnvironmentCube && frame.pipelineOptions->environment)
				{
					auto const& environment = frame.pipelineOptions->environment;
					auto resource = environment->environmentMap ? environment->environmentMap : environment->backgroundMap;
					frame.renderSystem->renderEnvironmentDebugCube(dynamic_cast<Texture*>(resource.get()), frame.camera.get());
				}
				if (frame.scene && frame.scene->show3dModels() && frame.sceneRenderPass && !frame.visibleModels.empty())
				{
					frame.sceneRenderPass->render(frame.visibleModels, frame.camera);
					frame.renderSystem->flushVertexBuffers();
				}
			}
		};
	}

	uint32_t bloomBlurIteration(UniformCollection const& parameters, std::string const& passName)
	{
		auto const& values = parameters.getUniformData();
		auto const found = values.find("ITERATION");
		if (found != values.end() && found->second.size >= sizeof(int32_t))
		{
			auto const authored = *reinterpret_cast<int32_t const*>(found->second.data);
			if (authored >= 0) return (uint32_t)authored;
		}
		return trailingPassIndex(passName);
	}

	void registerBuiltInRenderGraphPasses(RenderGraphPassFactoryRegistry& registry)
	{
		auto shadow = metadata("Shadow Depth", "Shadows", GraphPassType::Scene);
		shadow.outputs.push_back({ "Depth", true, true, depthFormats() });
		registry.registerScenePassFactory("MPP.ShadowDepth", [] { return std::make_unique<ShadowDepthPass>(); }, shadow);

		auto extract = metadata("Bloom Extract", "Bloom", GraphPassType::Fullscreen);
		extract.inputs.push_back({ "Scene HDR", "TEX1", true, colourFormats(), {} });
		extract.outputs.push_back({ "Bloom", false, true, colourFormats() });
		extract.parameters.push_back({ "THRESHOLD", program::GLSLType::Float, 1, 1, false, true, 0.0, 100.0, "exposure" });
		registry.registerScenePassFactory("MPP.BloomExtract", [] { return std::make_unique<BloomExtractPass>(); }, extract);

		auto blur = metadata("Bloom Blur Horizontal", "Bloom", GraphPassType::Fullscreen);
		blur.inputs.push_back({ "Input", "TEX1", true, colourFormats(), {} }); blur.outputs.push_back({ "Output", false, true, colourFormats() });
		// Which blur level this pass is. Compared against the bloom blurPasses option
		// to decide whether the pass blurs or just copies through.
		blur.parameters.push_back({ "ITERATION", program::GLSLType::Int, 1, 1, false, true, 0.0, 15.0, "count" });
		blur.nameDerivedFallbackParameter = "ITERATION";
		registry.registerScenePassFactory("MPP.BloomBlurHorizontal", [] { return std::make_unique<BloomBlurPass>(true); }, blur);
		blur.displayName = "Bloom Blur Vertical";
		registry.registerScenePassFactory("MPP.BloomBlurVertical", [] { return std::make_unique<BloomBlurPass>(false); }, blur);

		auto composite = metadata("Bloom Composite", "Bloom", GraphPassType::Fullscreen);
		composite.inputs.push_back({ "Scene", "SCENE", true, colourFormats(), {} }); composite.inputs.push_back({ "Bloom", "BLOOM", true, colourFormats(), {} });
		composite.outputs.push_back({ "Composite", false, true, colourFormats() });
		composite.parameters.push_back({ "INTENSITY", program::GLSLType::Float, 1, 1, false, true, 0.0, 10.0, "intensity" });
		registry.registerScenePassFactory("MPP.BloomComposite", [] { return std::make_unique<BloomCompositePass>(); }, composite);

		auto present = metadata("Tone Map Presentation", "Presentation", GraphPassType::Present);
		present.inputs.push_back({ "HDR Input", "TEX1", true, colourFormats(), {} }); present.outputs.push_back({ "Presentation", false, true, colourFormats() });
		present.parameters.push_back({ "EXPOSURE", program::GLSLType::Float, 1, 1, false, true, 0.0, 100.0, "exposure" });
		present.parameters.push_back({ "TONE_MAP_OPERATOR", program::GLSLType::Int, 1, 1, false, true, 0.0, 1.0, "enum" });
		registry.registerScenePassFactory("MPP.ToneMapPresent", [] { return std::make_unique<ToneMapPresentPass>(); }, present);

		auto scene = metadata("PBR Scene", "Scene", GraphPassType::Scene);
		scene.inputs.push_back({ "Shadow", "SHADOW_MAP", false, depthFormats(), "NeutralShadow" });
		scene.outputs.push_back({ "HDR Colour", false, true, colourFormats() }); scene.outputs.push_back({ "Emissive MRT", false, false, colourFormats() }); scene.outputs.push_back({ "Depth", true, true, depthFormats() });
		scene.materialSlots.push_back("SceneMaterials");
		registry.registerScenePassFactory("MPP.PbrScene", [] { return std::make_unique<ScenePass>(); }, scene);
		scene.displayName = "Legacy Scene";
		registry.registerScenePassFactory("MPP.LegacyScene", [] { return std::make_unique<ScenePass>(); }, scene);

		auto customFullscreen = metadata("Custom Fullscreen", "Custom", GraphPassType::Fullscreen);
		customFullscreen.acceptsProgram = true;
		customFullscreen.allowAdditionalInputs = true;
		customFullscreen.allowAdditionalOutputs = true;
		customFullscreen.allowAdditionalParameters = true;
		registry.registerMetadata("MPP.CustomFullscreen", customFullscreen);

		auto customMaterialRaster = metadata("Custom Material Raster", "Custom", GraphPassType::Scene);
		customMaterialRaster.allowAdditionalInputs = true;
		customMaterialRaster.allowAdditionalOutputs = true;
		customMaterialRaster.allowAdditionalParameters = true;
		customMaterialRaster.materialSlots.push_back("SceneMaterials");
		registry.registerScenePassFactory("MPP.CustomMaterialRaster", [] { return std::make_unique<ScenePass>(); }, customMaterialRaster);
	}
}
