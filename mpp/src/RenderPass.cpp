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

			// renderModelBatched historically marks every model as blended. Preserve
			// that legacy behaviour for non-PBR materials in Default, but always
			// restore the authored alpha semantics for a PBR material. Otherwise an
			// OPAQUE PBR model shown through Default has depth writes disabled and its
			// back-facing triangles can draw over its front-facing triangles.
			for (auto meshInstance : instance->getMeshInstances())
			{
				auto material = static_cast<Material*>(meshInstance->getMaterial().get());
				if (!mPbrForward && !material->isPbr())
				{
					continue;
				}

				// Render opaque and masked PBR materials with depth writes. Blend
				// materials are drawn later by the transparent sort.
				bool transparent = material->isPbr() &&
					material->getPbrSurface().alphaMode == MaterialSpecification::PbrAlphaMode::Blend;
				meshInstance->blend(transparent);
				meshInstance->sortTransparent(transparent);
			}
		}
	}
}