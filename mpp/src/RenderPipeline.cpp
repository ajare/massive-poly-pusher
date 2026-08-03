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

		// Post effects
		mRenderSystem->setProjection2dOrthographic();

		for (auto const& effect: mPostEffects)
		{
			auto const* pe = static_cast<PostEffect const*>(effect.get());
		}

		// Render to screen
		mRenderSystem->resetTransform();
		mRenderSystem->renderToScreen();
		mRenderSystem->clearScreen(scene->getClearColour());

		auto outputRenderTexture = static_cast<RenderTexture*>(getOutputRenderTarget().get());
		if (mOptions.mode == RenderPipelineMode::PbrForward)
		{
			mRenderSystem->renderToneMappedFullscreenQuad(outputRenderTexture, mOptions.exposure, mOptions.toneMapOperator == PbrToneMapOperator::Aces);
		}
		else
		{
			mRenderSystem->renderFullscreenQuad(outputRenderTexture, mpp::BlendMode::One, mpp::BlendMode::Zero);
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