#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#	include <Windows.h>
#endif

#include <gl/GL.h>

#include <cassert>

#include "mpp/ProgrammaticTextureAtlasStream.h"

using namespace std;

namespace mpp
{

	ProgrammaticTextureAtlasStream::ProgrammaticTextureAtlasStream(ResourceManager* resourceMgr)
		: TextureAtlasStream(resourceMgr)
	{
	}

	void ProgrammaticTextureAtlasStream::setTargetFormat(InternalType type, bool normalized, size_t bitSize, size_t channels)
	{
		//mParams.internalFormat = format;
	}

	void ProgrammaticTextureAtlasStream::setData(uint8_t const* data, size_t width, size_t height, size_t bitsPerPixel, uint32_t pixelFormat, uint32_t dataType)
	{
		assert((bitsPerPixel == 24 || bitsPerPixel == 32) && "ProgrammaticTextureStream::setData() 'bitsPerPixel' is invalid.");

		mData.width = width;
		mData.height = height;
		mData.bitsPerPixel = bitsPerPixel;
		mData.pixelFormat = pixelFormat;
		mData.dataType = dataType;

		auto dataSize = getDataSize();
		mData.data = new uint8_t[dataSize];
		memcpy(mData.data, data, dataSize);
	}

	void ProgrammaticTextureAtlasStream::setFile(std::string const& filename, ImageLoadFunction loader)
	{
		mSource = filename;
		mLoadFunc = loader;
	}

	void ProgrammaticTextureAtlasStream::setFiltered(bool filtered)
	{
		mParams.minFilter = mParams.magFilter = filtered ? GL_LINEAR : GL_NEAREST;
	}

}