#include "mpp/Config.h"

#include "mpp/RenderSystem.h"
#include "mpp/PostEffect.h"
#include "mpp/PostEffectStream.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{
	/*
	 * Constructor.
	 *
	 */
	PostEffect::PostEffect(string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream)
		: Resource(name, "PostEffect", renderSystem, resourceMgr, resourceStream)
	{
	}

	void PostEffect::createImpl()
	{
		PostEffectStream* peStr = dynamic_cast<PostEffectStream*>(getResourceStream().get());
		if (!peStr)
		{
			THROW_MPP("Could not cast to type 'PostEffectStream'.", __LINE__, __FILE__, __func__);
		}

		// Create output RenderTexture
		// ...
	}

	void PostEffect::destroyImpl()
	{
	}

	void PostEffect::loadImpl()
	{
	}

	void PostEffect::unloadImpl()
	{
	}

	RenderTargetPtr PostEffect::getOuputRenderTarget()
	{
		THROW_IF_NOT_LOADED;

		return mOutput.target;
	}
}