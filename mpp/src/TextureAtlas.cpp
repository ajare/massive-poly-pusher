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
	{
	}

	string const& TextureAtlas::getType() const
	{
		return mAtlasType;
	}

	void TextureAtlas::createImpl()
	{
		Texture::createImpl();

		TextureAtlasStream* tStr = dynamic_cast<TextureAtlasStream*>(getResourceStream().get());
		if (!tStr)
		{
			THROW_MPP("Could not cast to type 'TextureAtlasStream'.", __LINE__, __FILE__, __func__);
		}

		// Add tiles
		auto const& tiles = tStr->getTiles();
		for (auto const& tile: tiles)
		{
			Tile t;
			
			t.u[0] = tile.second.u[0];
			t.v[0] = tile.second.v[0];
			t.u[1] = tile.second.u[1];
			t.v[1] = tile.second.v[1];

			mTiles[tile.first] = t;
		}
	}

	TextureAtlas::Tile const& TextureAtlas::getTile(string const& name) const
	{
		return mTiles.at(name);
	}
}