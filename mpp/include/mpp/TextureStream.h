#pragma once

#include <string>
#include <functional>

#include "mpp/ResourceStream.h"

namespace mpp
{
	struct TextureData
	{
		uint8_t* data{ nullptr };
		int width, height, bitsPerPixel;
	};

	class TextureStream;

	typedef std::function<TextureData(std::string const&)> ImageLoadFunction;

	class _MPPAPI TextureStream : public ResourceStream
	{
		uint8_t* mData;

		int mWidth, mHeight, mBitsPerPixel;

		bool mFiltered;

		ImageLoadFunction mLoadFunc;

		std::string mSource;

	private:

		void loadImpl();

	public:

		TextureStream(ResourceManager* resourceMgr, uint8_t const* data, int width, int height, int bitsPerPixel, bool filtered, std::string streamType = "Texture");

		TextureStream(ResourceManager* resourceMgr, std::string const& filename, ImageLoadFunction loader, bool filtered, std::string streamType = "Texture");

		virtual ~TextureStream();

		uint8_t const* getData() const;

		int getWidth() const;

		int getHeight() const;

		int getBitsPerPixel() const;

		bool isFiltered() const;

		int getDataSize() const;
	};
}