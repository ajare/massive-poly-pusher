#include "mpp/RenderPass.h"
#include "mpp/RenderSystem.h"
#include "mpp/Material.h"
#include "mpp/MppException.h"
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

			// Preserve the legacy Default blend behaviour for BasicMaterial. PBR
			// alpha semantics are supplied by the future PbrMaterial implementation
			// through Material's common render classification.
			for (auto meshInstance : instance->getMeshInstances())
			{
				auto material = static_cast<Material*>(meshInstance->getMaterial().get());
				if (!mPbrForward && material->getShadingModel() == Material::ShadingModel::Pbr)
				{
					THROW_MPP("PbrMaterial requires a PBR forward pipeline.", __LINE__, __FILE__, __func__);
				}
				if (!mPbrForward)
				{
					continue; // Preserve legacy BasicMaterial blend behaviour in Default.
				}
				bool const transparent = material->getShadingModel() == Material::ShadingModel::Pbr && material->isTransparent();
				meshInstance->blend(transparent);
				meshInstance->sortTransparent(transparent);
			}
		}
	}
}