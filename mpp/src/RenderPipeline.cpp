#include <algorithm>

#include "mpp/RenderPipeline.h"
#include "mpp/RenderSystem.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{

	RenderPipeline::RenderPipeline(string const& name, RenderSystem* renderSystem, RenderPipelineOptions const& options)
		: mName(name)
		, mRenderSystem(renderSystem)
		, mOptions(options)
	{
		// The PBR preview path owns an HDR scene target. Legacy pipelines keep
		// their RGBA8 target and existing presentation behaviour.
		mPasses.push_back(make_shared<RenderPass>(renderSystem, mOptions.mode == RenderPipelineMode::PbrForward));
	}

	RenderPipeline::~RenderPipeline()
	{
	}

	string const& RenderPipeline::getName() const
	{
		return mName;
	}

	RenderPipelineOptions const& RenderPipeline::getOptions() const
	{
		return mOptions;
	}

	void RenderPipeline::setExposure(float exposure)
	{
		mOptions.exposure = std::max(exposure, 0.0f);
	}

	void RenderPipeline::setToneMapOperator(PbrToneMapOperator toneMapOperator)
	{
		mOptions.toneMapOperator = toneMapOperator;
	}

	void RenderPipeline::setBloomOptions(BloomOptions const& bloomOptions)
	{
		mOptions.bloom = bloomOptions;
	}

	void RenderPipeline::ensureBloomTargets(size_t width, size_t height)
	{
		if (!mOptions.bloom.enabled)
		{
			mBloomExtractTarget.reset();
			mBloomPingTarget.reset();
			mBloomPongTarget.reset();
			mBloomCompositeTarget.reset();
			return;
		}

		auto needsCreate = [&](RenderTargetPtr const& target)
		{
			return !target || target->getWidth() != width || target->getHeight() != height;
		};
		if (!needsCreate(mBloomExtractTarget))
		{
			return;
		}

		RenderTextureOptions options;
		options.numAttachments = 1;
		options.colourType = TextureInternalType::Float;
		options.colourNormalised = false;
		options.colourBitSize = 16;
		options.params.minFilter = GL_LINEAR;
		options.params.magFilter = GL_LINEAR;
		options.params.wrap = GL_CLAMP_TO_EDGE;
		mBloomExtractTarget = mRenderSystem->createRenderTexture(mName + ".BloomExtract", width, height, options);
		mBloomPingTarget = mRenderSystem->createRenderTexture(mName + ".BloomPing", width, height, options);
		mBloomPongTarget = mRenderSystem->createRenderTexture(mName + ".BloomPong", width, height, options);
		mBloomCompositeTarget = mRenderSystem->createRenderTexture(mName + ".BloomComposite", width, height, options);
	}

	void RenderPipeline::setPbrEnvironment(PbrEnvironmentPtr environment)
	{
		mOptions.environment = std::move(environment);
	}

	void RenderPipeline::setShadowDomain(string const& shadowDomain)
	{
		mOptions.shadowDomain = shadowDomain;
	}

	RenderTargetPtr RenderPipeline::getOutputRenderTarget()
	{
		if (mOptions.bloom.enabled && mBloomCompositeTarget)
		{
			return mBloomCompositeTarget;
		}
		if (mPostEffects.empty())
		{
			return mPasses.back()->getRenderTarget();
		}
		else
		{
			return static_cast<PostEffect*>(mPostEffects.back().get())->getOuputRenderTarget();
		}
	}

	void RenderPipeline::resize(size_t width, size_t height)
	{
		for (auto const& pass : mPasses)
		{
			pass->resize(width, height);
		}
	}

	void RenderPipeline::addRenderPass(RenderPassPtr pass)
	{
		mPasses.push_back(pass);
	}

	void RenderPipeline::addPostEffect(ResourcePtr effect)
	{
		mPostEffects.push_back(effect);
	}

	void RenderPipeline::render(ScenePtr scene, CameraPtr camera, glm::vec2 const& offset2d)
	{
		// Set viewport
		auto const& viewport = scene->getViewport();
		mRenderSystem->setViewport(viewport.x, viewport.y, (size_t)viewport.width, (size_t)viewport.height);

		// Lazily create shadow-domain resources for any participating pipeline.
		// The depth pass and shader consumption follow in later shadow milestones.
		if (!mOptions.shadowDomain.empty())
		{
			mRenderSystem->ensureShadowDomainResources(mOptions.shadowDomain);
		}

		// Scene passes
		mRenderSystem->setProjection3dPerspective(
			camera->getFov(),
			camera->getNearClipDistance(),
			camera->getFarClipDistance());
		

		auto const& models = scene->get3dModelsInView(camera);
		if (!mOptions.shadowDomain.empty())
		{
			mRenderSystem->renderShadowDomain(mOptions.shadowDomain, models);
		}
		mRenderSystem->setActiveShadowDomain(mOptions.shadowDomain);

		map<string, ResourcePtr> pipelineSamplerOverrides;
		if (mOptions.mode == RenderPipelineMode::PbrForward)
		{
			mRenderSystem->setActivePbrEnvironment(mOptions.environment);
			if (mOptions.environment)
			{
				pipelineSamplerOverrides["PBR_IRRADIANCE_MAP"] = mOptions.environment->irradianceMap;
				pipelineSamplerOverrides["PBR_PREFILTERED_SPECULAR_MAP"] = mOptions.environment->prefilteredSpecularMap;
				pipelineSamplerOverrides["PBR_BRDF_LUT"] = mOptions.environment->brdfIntegrationLut;
			}
		}
		mRenderSystem->setActivePipelineSamplerOverrides(pipelineSamplerOverrides);
		for (auto const& pass : mPasses)
		{
			// Start pass
			pass->bindRenderTarget();

			// Clear
			mRenderSystem->clearScreen(scene->getClearColour());

			// Render pass
			if (!models.empty() && scene->show3dModels())
			{
				pass->render(models, camera);

				// Flush
				mRenderSystem->flushVertexBuffers();
			}
		}
		mRenderSystem->setActivePipelineSamplerOverrides({});
		mRenderSystem->setActiveShadowDomain("");
		if (mOptions.mode == RenderPipelineMode::PbrForward)
		{
			mRenderSystem->setActivePbrEnvironment(nullptr);
		}

		// Reset viewport
		mRenderSystem->resetViewport();

		// Pipeline image effects run after all material shading. PBR bloom is
		// therefore composed in HDR before tone mapping; legacy uses the same
		// effect sequence on its completed LDR scene target.
		mRenderSystem->setProjection2dOrthographic();
		mRenderSystem->resetTransform();
		auto sceneTexture = static_cast<RenderTexture*>(mPasses.back()->getRenderTarget().get());
		Texture* presentationTexture = sceneTexture;
		ensureBloomTargets(sceneTexture->getWidth(), sceneTexture->getHeight());
		if (mOptions.bloom.enabled)
		{
			mRenderSystem->setRenderTarget(mBloomExtractTarget);
			mRenderSystem->renderBloomExtract(sceneTexture, mOptions.bloom.threshold);

			Texture* blurredTexture = static_cast<RenderTexture*>(mBloomExtractTarget.get());
			for (uint32_t pass = 0; pass < mOptions.bloom.blurPasses; ++pass)
			{
				mRenderSystem->setRenderTarget(mBloomPingTarget);
				mRenderSystem->renderBloomBlur(blurredTexture, glm::vec2(1.0f, 0.0f));
				mRenderSystem->setRenderTarget(mBloomPongTarget);
				mRenderSystem->renderBloomBlur(static_cast<RenderTexture*>(mBloomPingTarget.get()), glm::vec2(0.0f, 1.0f));
				blurredTexture = static_cast<RenderTexture*>(mBloomPongTarget.get());
			}

			mRenderSystem->setRenderTarget(mBloomCompositeTarget);
			mRenderSystem->renderBloomCombine(sceneTexture, blurredTexture, mOptions.bloom.intensity);
			presentationTexture = static_cast<RenderTexture*>(mBloomCompositeTarget.get());
		}

		// Render to screen
		mRenderSystem->resetTransform();
		mRenderSystem->renderToScreen();
		mRenderSystem->clearScreen(scene->getClearColour());

		if (mOptions.mode == RenderPipelineMode::PbrForward)
		{
			mRenderSystem->renderToneMappedFullscreenQuad(presentationTexture, mOptions.exposure, mOptions.toneMapOperator == PbrToneMapOperator::Aces);
		}
		else
		{
			mRenderSystem->renderFullscreenQuad(presentationTexture, mpp::BlendMode::One, mpp::BlendMode::Zero);
		}

		// 2d models
		if (scene->show2dModels())
		{
			auto orderedModels = scene->get2dModelsInView();

			// Sort models.
			sort(orderedModels.begin(), orderedModels.end(), [](auto const& a, auto const& b)
			{
				return a.second < b.second;
			});

			mRenderSystem->pushModelMatrix();

			for (auto orderedModel: orderedModels)
			{
				auto model = orderedModel.first;

				auto const& origin = model->getOrigin();
				auto const& offset = model->getOffset();
				float angle = model->getAngle();
				float orbit = model->getOrbitAngle();
				auto const& scale = model->getScale();

				mRenderSystem->resetTransform();

				mRenderSystem->translateTransform2d(glm::vec2(-offset2d.x, -offset2d.y));

				// Scale and rotate object, then rotate around the origin, then move to world position.
				mRenderSystem->translateTransform2d(origin);
				mRenderSystem->rotateTransform2d(orbit);
				mRenderSystem->translateTransform2d(offset);
				mRenderSystem->rotateTransform2d(angle);
				mRenderSystem->scaleTransform2d(scale);

				model->render(camera);
			}

			mRenderSystem->popModelMatrix();

			// In case final model was batched and we're not rendering anything else this frame
			mRenderSystem->flushVertexBuffers();  
		}
	}
}