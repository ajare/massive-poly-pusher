#include <cassert>
#include <filesystem>

#include "utils/FileSystem.h"

#include "mpp/TextureStream.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	TextureStream::TextureStream(ResourceManager* resourceMgr, string streamType)
		: ResourceStream(resourceMgr, streamType)
	{
	}

	/*
	* Destructor.
	*
	*/
	TextureStream::~TextureStream()
	{
		delete[] mData.data;
	}

	/*
	 * Load data.
	 *
	 */
	void TextureStream::loadImpl()
	{
		if (mQualitySettings[mQualitySetting].loadFunc)
		{
			mData = mQualitySettings[mQualitySetting].loadFunc(mQualitySettings[mQualitySetting].source);
		}
	}

	uint32_t TextureStream::getInternalFormat() const
	{
		return mQualitySettings[mQualitySetting].internalFormat;
	}

	uint32_t TextureStream::getTarget() const
	{
		return mQualitySettings[mQualitySetting].target;
	}

	/*
	 * Get raw texture data.
	 *
	 */
	uint8_t const* TextureStream::getData() const
	{
		return mData.data;
	}

	/*
	 * Get texture width.
	 *
	 */
	size_t TextureStream::getWidth() const
	{
		return mData.width;
	}

	/*
	 * Get texture height.
	 *
	 */
	size_t TextureStream::getHeight() const
	{
		return mData.height;
	}

	size_t TextureStream::getDepth() const
	{
		return mData.depth;
	}

	/*
	 * Get texture bits per pixel.
	 *
	 */
	size_t TextureStream::getBitsPerPixel() const
	{
		return mData.bitsPerPixel;
	}

	uint32_t TextureStream::getPixelFormat() const
	{
		return mData.pixelFormat;
	}

	uint32_t TextureStream::getPixelDataType() const
	{
		return mData.dataType;
	}

	/*
	 * Get texture data size in bytes.
	 *
	 */
	size_t TextureStream::getDataSize() const
	{
		return getWidth() * getHeight() * getBitsPerPixel() / 8;
	}

	TextureParams const& TextureStream::getParams() const
	{
		return mQualitySettings[mQualitySetting].params;
	}

	string const& TextureStream::getSampler() const
	{
		return mQualitySettings[mQualitySetting].sampler;
	}

	map<string, TextureStream::Tile> const& TextureStream::getTiles() const
	{
		return mTiles;
	}

	uint32_t TextureStream::createQualitySetting(string const& name)
	{
		auto qualityId = mQualitySettings.size();
		mQualityNames[name] = qualityId;

		mQualitySettings.push_back(QualitySetting());
		return qualityId;
	}

	void TextureStream::setFileBasePaths(string const& basepath)
	{
		for (auto& qs: mQualitySettings)
		{
			filesystem::path sourceFp(qs.source);
			if (sourceFp.is_relative())
			{
				qs.source = utils::FileSystem::concatPaths(basepath, qs.source);
			}
		}
	}
}