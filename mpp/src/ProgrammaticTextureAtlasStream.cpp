#include <cassert>

#include "mpp/ProgrammaticTextureAtlasStream.h"

using namespace std;

namespace mpp
{

	ProgrammaticTextureAtlasStream::ProgrammaticTextureAtlasStream(ResourceManager* resourceMgr)
		: TextureAtlasStream(resourceMgr)
	{
	}

	void ProgrammaticTextureAtlasStream::setData(uint8_t const* data, size_t width, size_t height, size_t bitsPerPixel)
	{
		assert((bitsPerPixel == 24 || bitsPerPixel == 32) && "ProgrammaticTextureStream::setData() 'bitsPerPixel' is invalid.");

		mWidth = width;
		mHeight = height;
		mBitsPerPixel = bitsPerPixel;

		auto dataSize = getDataSize();
		mData = new uint8_t[dataSize];
		memcpy(mData, data, dataSize);
	}

	void ProgrammaticTextureAtlasStream::setFile(std::string const& filename, ImageLoadFunction loader)
	{
		mSource = filename;
		mLoadFunc = loader;
	}

	void ProgrammaticTextureAtlasStream::setFiltered(bool filtered)
	{
		mFiltered = filtered;
	}

}