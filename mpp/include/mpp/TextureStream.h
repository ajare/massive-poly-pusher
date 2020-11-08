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
	protected:

		uint8_t* mData;

		size_t mWidth, mHeight, mBitsPerPixel;

		bool mFiltered;

		ImageLoadFunction mLoadFunc;

		std::string mSource;

	private:

		void loadImpl();

	public:

		TextureStream(ResourceManager* resourceMgr, std::string streamType = "Texture");

		virtual ~TextureStream();

		uint8_t const* getData() const;

		size_t getWidth() const;

		size_t getHeight() const;

		size_t getBitsPerPixel() const;

		bool isFiltered() const;

		size_t getDataSize() const;
	};
}