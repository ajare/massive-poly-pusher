#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#	include <Windows.h>
#endif

#include <glew/glew.h>
#include <gl/GL.h>

#include <cassert>

#include "mpp/ProgrammaticTextureAtlasStream.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{

	ProgrammaticTextureAtlasStream::ProgrammaticTextureAtlasStream(ResourceManager* resourceMgr)
		: TextureAtlasStream(resourceMgr)
	{
	}

	void ProgrammaticTextureAtlasStream::setTargetFormat(InternalType type, bool normalized, size_t bitSize, size_t channels)
	{
		if (type == InternalType::Float && normalized)
		{
			// Ignore/warning
		}

		if (channels < 1 || channels > 4)
		{
			// Error
			THROW_MPP("Invalid texture channel count.", __LINE__, __FILE__, __func__);
		}

		switch (type)
		{
		case InternalType::UnsignedInteger:
			if (normalized)
			{
				switch (channels)
				{
				case 1:
					switch (bitSize)
					{
					case 8: mParams.internalFormat = GL_R8; break;
					case 16: mParams.internalFormat = GL_R16; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 2:
					switch (bitSize)
					{
					case 8: mParams.internalFormat = GL_RG8; break;
					case 16: mParams.internalFormat = GL_RG16; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 3:
					switch (bitSize)
					{
					case 4: mParams.internalFormat = GL_RGB4; break;
					case 5: mParams.internalFormat = GL_RGB5; break;
					case 8: mParams.internalFormat = GL_RGB8; break;
					case 10: mParams.internalFormat = GL_RGB10; break;
					case 12: mParams.internalFormat = GL_RGB12; break;
					case 16: mParams.internalFormat = GL_RGB16; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 4:
					switch (bitSize)
					{
					case 2: mParams.internalFormat = GL_RGBA2; break;
					case 4: mParams.internalFormat = GL_RGBA4; break;
					case 8: mParams.internalFormat = GL_RGBA8; break;
					case 12: mParams.internalFormat = GL_RGBA12; break;
					case 16: mParams.internalFormat = GL_RGBA16; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				}
			}
			else
			{
				switch (channels)
				{
				case 1:
					switch (bitSize)
					{
					case 8: mParams.internalFormat = GL_R8UI; break;
					case 16: mParams.internalFormat = GL_R16UI; break;
					case 32: mParams.internalFormat = GL_R32UI; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 2:
					switch (bitSize)
					{
					case 8: mParams.internalFormat = GL_RG8UI; break;
					case 16: mParams.internalFormat = GL_RG16UI; break;
					case 32: mParams.internalFormat = GL_RG32UI; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 3:
					switch (bitSize)
					{
					case 8: mParams.internalFormat = GL_RGB8UI; break;
					case 16: mParams.internalFormat = GL_RGB16UI; break;
					case 32: mParams.internalFormat = GL_RGB32UI; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 4:
					switch (bitSize)
					{
					case 8: mParams.internalFormat = GL_RGBA8UI; break;
					case 16: mParams.internalFormat = GL_RGBA16UI; break;
					case 32: mParams.internalFormat = GL_RGBA32UI; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				}
			}
			break;

		case InternalType::SignedInteger:
			if (normalized)
			{
				switch (channels)
				{
				case 1:
					switch (bitSize)
					{
					case 8: mParams.internalFormat = GL_R8_SNORM; break;
					case 16: mParams.internalFormat = GL_R16_SNORM; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 2:
					switch (bitSize)
					{
					case 8: mParams.internalFormat = GL_RG8_SNORM; break;
					case 16: mParams.internalFormat = GL_RG16_SNORM; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 3:
					switch (bitSize)
					{
					case 8: mParams.internalFormat = GL_RGB8_SNORM; break;
					case 16: mParams.internalFormat = GL_RGB16_SNORM; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 4:
					switch (bitSize)
					{
					case 8: mParams.internalFormat = GL_RGBA8_SNORM; break;
					case 16: mParams.internalFormat = GL_RGBA16_SNORM; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				}
			}
			else
			{
				switch (channels)
				{
				case 1:
					switch (bitSize)
					{
					case 8: mParams.internalFormat = GL_R8I; break;
					case 16: mParams.internalFormat = GL_R16I; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 2:
					switch (bitSize)
					{
					case 8: mParams.internalFormat = GL_RG8I; break;
					case 16: mParams.internalFormat = GL_RG16I; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 3:
					switch (bitSize)
					{
					case 8: mParams.internalFormat = GL_RGB8I; break;
					case 16: mParams.internalFormat = GL_RGB16I; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 4:
					switch (bitSize)
					{
					case 8: mParams.internalFormat = GL_RGBA8I; break;
					case 16: mParams.internalFormat = GL_RGBA16I; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				}
			}
			break;

		case InternalType::Float:
			switch (channels)
			{
			case 1:
				switch (bitSize)
				{
				case 16: mParams.internalFormat = GL_R16F; break;
				case 32: mParams.internalFormat = GL_R32F; break;
				default:
					THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
				}
				break;
			case 2:
				switch (bitSize)
				{
				case 16: mParams.internalFormat = GL_RG16F; break;
				case 32: mParams.internalFormat = GL_RG32F; break;
				default:
					THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
				}
				break;
			case 3:
				switch (bitSize)
				{
				case 16: mParams.internalFormat = GL_RGB16F; break;
				case 32: mParams.internalFormat = GL_RGB32F; break;
				default:
					THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
				}
				break;
			case 4:
				switch (bitSize)
				{
				case 16: mParams.internalFormat = GL_RGBA16F; break;
				case 32: mParams.internalFormat = GL_RGBA32F; break;
				default:
					THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
				}
				break;
			}
			break;

		default:
			// Error, unknown internal type
			THROW_MPP("Unknown texture format type.", __LINE__, __FILE__, __func__);
		}
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