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
			Texture1D,
			Texture2D,
			Texture3D,
			CubeMap
		};

		enum class Filtering
		{
			Nearest,
			Linear
		};

		enum class Wrapping
		{
			Repeat,
			MirroredRepeat,
			ClampToEdge,
			ClampToBorder
		};

		enum class InternalType
		{
			Auto,
			UnsignedInteger,
			SignedInteger,
			Float
		};

	private:

		struct QualitySetting
		{
			TextureParams params;
			std::string source;
		};

	protected:

		TextureData mData;

		std::vector<QualitySetting> mQualitySettings;

		uint32_t mInternalFormat;

		uint32_t mTarget;

		ImageLoadFunction mLoadFunc;

	protected:

		void loadImpl();

	public:

		TextureStream(ResourceManager* resourceMgr, std::string streamType = "Texture");

		virtual ~TextureStream();

		uint32_t getInternalFormat() const;

		uint32_t getTarget() const;

		uint8_t const* getData() const;

		size_t getWidth() const;

		size_t getHeight() const;

		size_t getDepth() const;

		size_t getBitsPerPixel() const;

		uint32_t getPixelFormat() const;

		uint32_t getPixelDataType() const;

		size_t getDataSize() const;

		TextureParams const& getParams(uint32_t quality) const;
	};
}