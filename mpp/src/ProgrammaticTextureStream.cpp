#include <cassert>

#include "mpp/ProgrammaticTextureStream.h"

using namespace std;

namespace mpp
{

	ProgrammaticTextureStream::ProgrammaticTextureStream(ResourceManager* resourceMgr)
		: TextureStream(resourceMgr)
	{
	}

	void ProgrammaticTextureStream::setData(uint8_t const* data, size_t width, size_t height, size_t bitsPerPixel)
	{
		assert((bitsPerPixel == 24 || bitsPerPixel == 32) && "ProgrammaticTextureStream::setData() 'bitsPerPixel' is invalid.");

		mWidth = width;
		mHeight = height;
		mBitsPerPixel = bitsPerPixel;

		auto dataSize = getDataSize();
		mData = new uint8_t[dataSize];
		memcpy(mData, data, dataSize);
	}

	void ProgrammaticTextureStream::setFile(std::string const& filename, ImageLoadFunction loader)
	{
		mSource = filename;
		mLoadFunc = loader;
	}

	void ProgrammaticTextureStream::setFiltered(bool filtered)
	{
		mFiltered = filtered;
	}

}