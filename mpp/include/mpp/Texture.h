#pragma once

#include "mpp/Resource.h"
#include "mpp/TextureData.h"

namespace mpp
{
	class _MPPAPI Texture : public Resource
	{
		TextureData mData;

		TextureParams mParams;

		uint32_t mTarget;

		uint32_t mInternalFormat;

		uint32_t mSortId;

	protected:

		void createImpl();

		void destroyImpl();

		void loadImpl();

		void unloadImpl();

	public:

		Texture(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		virtual int getWidth() const;

		virtual int getHeight() const;

		virtual int getBitsPerPixel() const;

		virtual void bind(int unit);

		void setSortId(uint32_t sortId);

		uint32_t getSortId() const;
	};

}
