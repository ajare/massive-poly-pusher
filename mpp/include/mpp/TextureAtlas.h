#pragma once

#include <map>

#include "mpp/Texture.h"

namespace mpp
{
	class _MPPAPI TextureAtlas : public Texture
	{
	public:

		struct Tile
		{
			float u[2];
			float v[2];
		};

	private:

		std::string mAtlasType;

		std::map<std::string, Tile> mTiles;

	protected:

		void createImpl();

	public:

		TextureAtlas(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		std::string const& getType() const;

		Tile const& getTile(std::string const& name) const;
	};

}
