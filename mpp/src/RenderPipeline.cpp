#include "mpp/RenderPipeline.h"
#include "mpp/RenderSystem.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{

	RenderPipeline::RenderPipeline(string const& name, RenderSystem* renderSystem)
		: mName(name)
		, mRenderSystem(renderSystem)
	{
		// Default pass
		mPasses.push_back(make_shared<RenderPass>(renderSystem));
	}

	RenderPipeline::~RenderPipeline()
	{
	}

	string const& RenderPipeline::getName() const
	{
		return mName;
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

		// Scene passes
		mRenderSystem->setProjection3dPerspective(
			camera->getFov(),
			camera->getNearClipDistance(),
			camera->getFarClipDistance());
		

		auto const& models = scene->get3dModelsInView(camera);
		if (!models.empty())
		{
			for (auto const& pass : mPasses)
			{
				// Start pass
				pass->bindRenderTarget();

				// Clear
				mRenderSystem->clearScreen(scene->getClearColour());

				// Render pass
				if (scene->show3dModels())
				{
					pass->render(models, camera);
				}

				// Flush
				mRenderSystem->flushVertexBuffers();
			}
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

		auto outputRenderTexture = static_cast<RenderTexture*>(getOutputRenderTarget().get());
		mRenderSystem->renderFullscreenQuad(outputRenderTexture, 0, mpp::BlendMode::One, mpp::BlendMode::Zero);

		// 2d models
		if (scene->show2dModels())
		{
			auto const& models = scene->get2dModelsInView();

			mRenderSystem->pushModelMatrix();
			mRenderSystem->translateTransform2d(glm::vec2(-offset2d.x, -offset2d.y));

			for (auto model: models)
			{
				auto const& origin = model->getOrigin();
				auto const& offset = model->getOffset();
				float angle = model->getAngle();
				float orbit = model->getOrbitAngle();
				auto const& scale = model->getScale();

				mRenderSystem->resetTransform();

				// Scale and rotate object, then rotate around the origin, then move to world position.
				mRenderSystem->translateTransform2d(origin);
				mRenderSystem->rotateTransform2d(orbit);
				mRenderSystem->translateTransform2d(offset);
				mRenderSystem->rotateTransform2d(angle);
				mRenderSystem->scaleTransform2d(scale);

				model->render();
			}
			mRenderSystem->popModelMatrix();
		}
	}
}