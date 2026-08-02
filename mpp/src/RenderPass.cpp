#include "mpp/RenderPass.h"
#include "mpp/RenderSystem.h"
#include "mpp/Material.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{

	RenderPass::RenderPass(RenderSystem* renderSystem, bool pbrForward)
		: mRenderSystem(renderSystem)
		, mPbrForward(pbrForward)
	{
		RenderTextureOptions options;
		options.numAttachments = 1;
		options.depthAttachment = RenderTextureDepthAttachment::DepthRenderbuffer;
		if (mPbrForward)
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

			if (mPbrForward)
			{
				// Render opaque and masked PBR materials first with depth writes.
				// Blend materials are rendered afterwards by the transparent sort.
				for (auto meshInstance : instance->getMeshInstances())
				{
					auto material = static_cast<Material*>(meshInstance->getMaterial().get());
					bool transparent = material->isPbr() &&
						material->getPbrSurface().alphaMode == MaterialSpecification::PbrAlphaMode::Blend;
					meshInstance->blend(transparent);
					meshInstance->sortTransparent(transparent);
				}
			}
		}
	}
}