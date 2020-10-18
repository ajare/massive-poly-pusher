#pragma once

#include <string>
#include <functional>

#include "mpp/ResourceStream.h"

namespace mpp
{
	struct TextureData
	{
		uint8* data{ nullptr };
		int width, height, bitsPerPixel;
	};

	class TextureStream;

	typedef std::function<TextureData(std::string const&)> ImageLoadFunction;

	class _MPPAPI TextureStream : public ResourceStream
	{
		uint8* mData;

		int mWidth, mHeight, mBitsPerPixel;

		bool mFiltered;

	private:

		void loadImpl();

	public:

		TextureStream(ResourceManager* resourceMgr, uint8 const* data, int width, int height, int bitsPerPixel, bool filtered);

		TextureStream(ResourceManager* resourceMgr, std::string const& filename, ImageLoadFunction loader, bool filtered);

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