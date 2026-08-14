#include <algorithm>
#include <string>

#include "mpp/RenderGraphBuiltInPasses.h"
#include "mpp/RenderGraphExecutor.h"
#include "mpp/RenderGraphPassFactoryRegistry.h"
#include "mpp/RenderGraphScenePass.h"
#include "mpp/RenderSystem.h"
#include "mpp/RenderPipeline.h"
#include "mpp/ResourceManager.h"
#include "mpp/PostEffectMaterial.h"
#include "mpp/MppException.h"

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

		// The one generic post-effect-chain pass type: a fullscreen quad shaded by
		// whichever PostEffectMaterial the authored pass names via programResource,
		// with sampler inputs bound by name (RenderGraphExecutionContext's existing
		// generic mechanism) and default uniforms taken from the material,
		// overridden by any graph-authored/executor-overridden parameters. Adding a
		// new post effect is therefore a PostEffectMaterial + shader, never a new
		// pass class -- this replaced the old bespoke BloomExtractPass/BloomBlurPass/
		// BloomCompositePass/ToneMapPresentPass classes (see doc/POST_EFFECT_CHAIN_IMPLEMENTATION_PLAN.md).
		class FullscreenEffectPass final : public RenderGraphScenePass
		{
		public:
			void execute(RenderGraphExecutionContext const& context) override
			{
				auto const& frame = context.getFrame();
				auto const& pass = context.getPass();
				Texture* primary = input(context, 0);
				bool const enabled = integerParameter(context, "ENABLED", 1) != 0;
				if (!enabled || pass.programResource.empty())
				{
					// Copy-through: a disabled effect must not break the chain for
					// whatever reads its output next (matches the existing bloom-blur
					// disabled behaviour this pass generalizes).
					frame.renderSystem->renderFullscreenQuad(primary, BlendMode::One, BlendMode::Zero);
					return;
				}
				// A PbrPipelineDocument-authored pass can only name its own
				// LocalResources by their bare authored name -- the document has no
				// way to predict the dynamically-generated root
				// (RenderPipelineOptions::resourceRoot) they end up registered
				// under. Try that qualified name first, matching
				// PbrPipelineRuntime::resolve()'s exact fallback order, before
				// falling back to a plain global lookup (DemoSuite's XmlGraphPBR
				// declares its materials as global names and never sets
				// resourceRoot, so it always takes this fallback).
				auto* resourceMgr = frame.renderSystem->getResourceManager();
				ResourcePtr materialResource;
				if (frame.pipelineOptions && !frame.pipelineOptions->resourceRoot.empty())
					materialResource = resourceMgr->getResource(frame.pipelineOptions->resourceRoot + "/" + pass.programResource, true);
				if (!materialResource) materialResource = resourceMgr->getResource(pass.programResource);
				materialResource->load();
				auto* material = dynamic_cast<PostEffectMaterial*>(materialResource.get());
				if (!material)
					THROW_MPP("FullscreenEffectPass '" + pass.name + "' programResource '" + pass.programResource + "' is not a PostEffectMaterial.", __LINE__, __FILE__, __func__);

				std::vector<std::pair<std::string, Texture*>> samplers;
				for (auto const& binding : pass.samplerBindings)
					samplers.push_back({ binding.sampler, dynamic_cast<Texture*>(context.getImage(binding.image).get()) });

				// Material defaults, then per-pass overrides on top -- the same layering
				// setPostEffectParameter/setPassParameterOverrides already give every
				// other graph pass.
				UniformCollection parameters = material->getUniforms();
				for (auto const& [name, value] : context.getParameters().getUniformData())
					parameters.setUniform(name, value.type, value.count, value.numElements, value.data);

				frame.renderSystem->renderGraphFullscreen(material->getProgram(), samplers, parameters);
			}
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

	void registerBuiltInRenderGraphPasses(RenderGraphPassFactoryRegistry& registry)
	{
		auto shadow = metadata("Shadow Depth", "Shadows", GraphPassType::Scene);
		shadow.outputs.push_back({ "Depth", true, true, depthFormats() });
		registry.registerScenePassFactory("MPP.ShadowDepth", [] { return std::make_unique<ShadowDepthPass>(); }, shadow);

		auto fullscreenEffect = metadata("Post Effect", "Post Effects", GraphPassType::Fullscreen);
		fullscreenEffect.inputs.push_back({ "Input", "TEX0", true, colourFormats(), {} });
		fullscreenEffect.outputs.push_back({ "Output", false, true, colourFormats() });
		fullscreenEffect.parameters.push_back({ "ENABLED", program::GLSLType::Int, 1, 1, false, false, 0.0, 1.0, "bool" });
		fullscreenEffect.acceptsProgram = true;
		fullscreenEffect.allowAdditionalInputs = true;
		fullscreenEffect.allowAdditionalParameters = true;
		registry.registerScenePassFactory("MPP.FullscreenEffect", [] { return std::make_unique<FullscreenEffectPass>(); }, fullscreenEffect);

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
