#include "mpp/Config.h"
#include "mpp/MppException.h"
#include "mpp/TextureAtlas.h"
#include "mpp/TextureAtlasStream.h"

using namespace std;

namespace mpp
{
	/*
	 * Constructor.
	 *
	 */
	TextureAtlas::TextureAtlas(string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream)
		: Texture(name, renderSystem, resourceMgr, resourceStream)
		, mAtlasType("TextureAtlas")
		, mImagesX(0)
		, mImagesY(0)
	{
	}

	string const& TextureAtlas::getType() const
	{
		return mAtlasType;
	}

	void TextureAtlas::setImageCounts(size_t x, size_t y)
	{
		mImagesX = x;
		mImagesY = y;
	}

	void TextureAtlas::createImpl()
	{
		Texture::createImpl();

		TextureAtlasStream* tStr = dynamic_cast<TextureAtlasStream*>(getResourceStream().get());
		if (!tStr)
		{
			THROW_MPP("Could not cast to type 'TextureAtlasStream'.", __LINE__, __FILE__, __func__);
		}

		mImagesX = tStr->getImagesX();
		mImagesY = tStr->getImagesY();
	}

	size_t TextureAtlas::getImagesX() const
	{
		return mImagesX;
	}

	size_t TextureAtlas::getImagesY() const
	{
		return mImagesY;
	}

}