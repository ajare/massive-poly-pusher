#include "mpp/RenderPass.h"
#include "mpp/RenderSystem.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{

	RenderPass::RenderPass(RenderSystem* renderSystem)
		: mRenderSystem(renderSystem)
	{
		// Scene target
		mTarget = renderSystem->createRenderTexture(
			"SceneTarget",
			renderSystem->getWindowWidth(),
			renderSystem->getWindowHeight(),
			1,
			true);
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

	void RenderPass::render(vector<SceneModel3dPtr> const& models, CameraPtr camera)
	{
		// Render models in view
		for (auto model: models)
		{
			auto instance = mRenderSystem->renderModelBatched(
				model->getModel(),
				model->getTransform(),
				camera);

			instance->setParams(model->getParams());
		}
	}
}