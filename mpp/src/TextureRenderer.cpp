#include "mpp/TextureRenderer.h"
#include "mpp/RenderTextureStream.h"
#include "mpp/RenderSystem.h"
#include "mpp/ResourceManager.h"

using namespace std;

namespace mpp
{

	TextureRenderer::TextureRenderer(string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr)
		: mName(name)
		, mRenderSystem(renderSystem)
		, mResourceMgr(resourceMgr)
	{
	}

	ResourcePtr TextureRenderer::createRenderTexture(int width, int height)
	{
		auto rtStream = new RenderTextureStream(mResourceMgr, width, height, true, 1);
		auto rt = mResourceMgr->declareResource(mName, ResourceStreamPtr(rtStream));

		rt->load();

		updateRenderTexture(rt);
		return rt;
	}

	void TextureRenderer::updateRenderTexture(ResourcePtr renderTexture)
	{
		auto rt = dynamic_pointer_cast<RenderTarget, Resource>(renderTexture);

		auto width = rt->getWidth();
		auto height = rt->getHeight();

		mRenderSystem->pushRenderTarget(rt);
		mRenderSystem->pushModelMatrix();
		mRenderSystem->resetTransform();

		render(width, height);

		mRenderSystem->popModelMatrix();
		mRenderSystem->popRenderTarget();
	}

}
