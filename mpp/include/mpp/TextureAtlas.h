#pragma once

#include "mpp/Texture.h"

namespace mpp
{
	class _MPPAPI TextureAtlas : public Texture
	{
		std::string mAtlasType;

		size_t mImagesX, mImagesY;

	protected:

		void createImpl();

	public:

		TextureAtlas(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		std::string const& getType() const;

		void setImageCounts(size_t x, size_t y);

		size_t getImagesX() const;

		size_t getImagesY() const;
	};

}
