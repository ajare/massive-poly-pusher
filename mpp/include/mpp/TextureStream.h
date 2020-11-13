#pragma once

#include <string>
#include <functional>

#include "mpp/ResourceStream.h"
#include "mpp/TextureData.h"

namespace mpp
{

	class TextureStream;

	typedef std::function<TextureData(std::string const&)> ImageLoadFunction;

	class _MPPAPI TextureStream : public ResourceStream
	{
	public:

		enum class Target
		{
			Texture_1D,
			Texture_2D,
			Texture_3D,
			CubeMap
		};

		enum class InternalType
		{
			Auto,
			UnsignedInteger,
			SignedInteger,
			Float
		};

	protected:

		TextureData mData;

		TextureParams mParams;

		ImageLoadFunction mLoadFunc;

		std::string mSource;

	protected:

		void loadImpl();

	public:

		TextureStream(ResourceManager* resourceMgr, std::string streamType = "Texture");

		virtual ~TextureStream();

		uint8_t const* getData() const;

		size_t getWidth() const;

		size_t getHeight() const;

		size_t getBitsPerPixel() const;

		uint32_t getPixelFormat() const;

		uint32_t getPixelDataType() const;

		size_t getDataSize() const;

		TextureParams const& getParams() const;
	};
}