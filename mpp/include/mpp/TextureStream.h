#pragma once

#include "mpp/ResourceStream.h"

namespace mpp
{
	class _MPPAPI TextureStream : public ResourceStream
	{
		uint8* mData;

		int mWidth, mHeight, mBitsPerPixel;

		bool mFiltered;

	private:

		void loadImpl();

	public:

		TextureStream(uint8 const* data, int width, int height, int bitsPerPixel, bool filtered);

		virtual ~TextureStream();

		std::string getType();

		uint8 const* getData() const;

		int getWidth() const;

		int getHeight() const;

		int getBitsPerPixel() const;

		bool isFiltered() const;

		int getDataSize() const;
	};
}