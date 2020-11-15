#pragma once

#include <map>
#include <string>

#include "mpp/TextureStream.h"

namespace mpp
{
	class _MPPAPI TextureAtlasStream : public TextureStream
	{
		struct Tile
		{
			float u[2];
			float v[2];
		};

	private:

		std::map<std::string, Tile> mTiles;

	public:

		explicit TextureAtlasStream(ResourceManager* resourceMgr);

		void addTile(std::string const& name, int x, int y, int w, int h);

		std::map<std::string, Tile> const& getTiles() const;

	};
}