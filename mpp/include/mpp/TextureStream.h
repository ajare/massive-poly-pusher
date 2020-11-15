#pragma once

#include <string>
#include <functional>

#include "mpp/ResourceStream.h"
#include "mpp/TextureParams.h"
#include "mpp/TextureData.h"

namespace mpp
{

	class TextureStream;

	typedef std::function<TextureData(std::string const&)> ImageLoadFunction;

	class _MPPAPI TextureStream : public ResourceStream
	{
	public:

		struct Tile
		{
			float u[2];
			float v[2];
		};

	public:

		enum class Target
		{
			Texture1D,
			Texture2D,
			Texture3D,
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

		struct QualitySetting
		{
			TextureParams params;
			std::string sampler;
			std::string source;
			ImageLoadFunction loadFunc;
		};

	protected:

		std::map<std::string, Tile> mTiles;

		TextureData mData;

		std::vector<QualitySetting> mQualitySettings;

		uint32_t mInternalFormat;

		uint32_t mTarget;

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

		TextureParams const& getParams() const;

		std::string const& getSampler() const;

		std::map<std::string, Tile> const& getTiles() const;

		uint32_t createQualitySetting(std::string const& name);
	};
}