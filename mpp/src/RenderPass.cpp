#include "mpp/RenderPass.h"
#include "mpp/RenderSystem.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{

	RenderPass::RenderPass(RenderSystem* renderSystem, bool highDynamicRange)
		: mRenderSystem(renderSystem)
	{
		RenderTextureOptions options;
		options.numAttachments = 1;
		options.depthAttachment = RenderTextureDepthAttachment::DepthRenderbuffer;
		if (highDynamicRange)
		{
			options.colourType = TextureInternalType::Float;
			options.colourNormalised = false;
			options.colourBitSize = 16;
			options.params.minFilter = GL_LINEAR;
			options.params.magFilter = GL_LINEAR;
		}

		mTarget = renderSystem->createRenderTexture(
			"SceneTarget",
			renderSystem->getWindowWidth(),
			renderSystem->getWindowHeight(),
			options);
	}

	RenderPass::~RenderPass()
	{
	}

	RenderTargetPtr RenderPass::getRenderTarget()
	{
		return mTarget;
	}

	void RenderPass::bindRenderTarget()
	{
		mRenderSystem->setRenderTarget(mTarget);
	}

	bool RenderPass::resize(size_t width, size_t height)
	{
		return mTarget->resize(width, height);
	}

	void RenderPass::render(vector<SceneModel3dPtr> const& models, CameraPtr camera)
	{
		// Render models in view
		for (auto model: models)
		{
			auto instance = mRenderSystem->renderModelBatched(
				static_cast<Model const&>(*model->getModel().get()),
				model->getTransform(),
				camera);

			instance->setParams(model->getParams());
		}
	}
}