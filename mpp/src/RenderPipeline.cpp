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

	void RenderPipeline::render(ScenePtr scene, CameraPtr camera)
	{
		// Scene passes
		mRenderSystem->setProjection3dPerspective(
			camera->getFov(),
			camera->getNearClipDistance(),
			camera->getFarClipDistance());

		auto models = scene->getObjectsInView(camera);
		for (auto const& pass: mPasses)
		{
			// Start pass
			pass->bindRenderTarget();

			// Clear
			auto clearColour = scene->getClearColour();

			GL_CHECK(glClearColor(clearColour.red, clearColour.green, clearColour.blue, clearColour.alpha));
			GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

			// Render pass
			pass->render(models, camera);

			// Flush
			mRenderSystem->flushVertexBuffers();
		}

		// Post effects
		mRenderSystem->setProjection2dOrthographic();

		for (auto const& effect: mPostEffects)
		{
			auto const* pe = static_cast<PostEffect const*>(effect.get());
		}

		// Render to screen
		mRenderSystem->renderToScreen();

		auto outputRenderTexture = static_cast<RenderTexture*>(getOutputRenderTarget().get());
		mRenderSystem->renderFullscreenQuad(outputRenderTexture, 0, mpp::BlendMode::One, mpp::BlendMode::Zero);
	}
}