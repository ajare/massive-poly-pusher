#pragma once

#include "mpp/Resource.h"

namespace mpp
{
	class _MPPAPI Texture : public Resource
	{
		uint8* mData;

		int mWidth, mHeight, mBitsPerPixel;

		bool mFiltered;
		
		uint32 mSortId;

	private:

		void createImpl();

		void destroyImpl();

		void loadImpl();

		void unloadImpl();

	public:

		Texture(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		virtual int getWidth() const;

		virtual int getHeight() const;

		virtual int getBitsPerPixel() const;

		void bind(int unit);

		void setSortId(uint32 sortId);

		uint32 getSortId() const;

		void setTexel(int x, int y, uint8 red, uint8 green, uint8 blue);
	};

}
