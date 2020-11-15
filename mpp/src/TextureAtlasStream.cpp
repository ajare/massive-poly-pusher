#include <cassert>

#include "mpp/TextureAtlasStream.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	TextureAtlasStream::TextureAtlasStream(ResourceManager* resourceMgr)
		: TextureStream(resourceMgr, "TextureAtlas")
	{
		mQualitySettings.resize(1);
	}

	void TextureAtlasStream::addTile(string const& name, int x, int y, int w, int h)
	{
		Tile t;
		
		t.u[0] = x / (float)getWidth();
		t.v[0] = y / (float)getHeight();

		t.u[1] = t.u[0] + w / (float)getWidth();
		t.v[1] = t.v[0] + h / (float)getHeight();

		mTiles[name] = t;
	}

	map<string, TextureAtlasStream::Tile> const& TextureAtlasStream::getTiles() const
	{
		return mTiles;
	}

}