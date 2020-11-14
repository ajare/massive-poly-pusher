#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#	include <Windows.h>
#endif

#include <glew/glew.h>
#include <gl/GL.h>

#include <cassert>

#include "mpp/ProgrammaticTextureStream.h"
#include "mpp/ResourceManager.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{

	ProgrammaticTextureStream::ProgrammaticTextureStream(ResourceManager* resourceMgr)
		: TextureStream(resourceMgr)
	{
		mQualitySettings.resize(1);
	}

	void ProgrammaticTextureStream::setInternalFormat(InternalType type, bool normalized, size_t bitSize, size_t channels)
	{
		if (type == InternalType::Float && normalized)
		{
			getResourceMgr()->warnMessage("ProgrammaticTextureStream: ignoring 'normalized': specified with floating-point data.");
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
					case 8: mInternalFormat = GL_R8; break;
					case 16: mInternalFormat = GL_R16; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 2:
					switch (bitSize)
					{
					case 8: mInternalFormat = GL_RG8; break;
					case 16: mInternalFormat = GL_RG16; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 3:
					switch (bitSize)
					{
					case 4: mInternalFormat = GL_RGB4; break;
					case 5: mInternalFormat = GL_RGB5; break;
					case 8: mInternalFormat = GL_RGB8; break;
					case 10: mInternalFormat = GL_RGB10; break;
					case 12: mInternalFormat = GL_RGB12; break;
					case 16: mInternalFormat = GL_RGB16; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 4:
					switch (bitSize)
					{
					case 2: mInternalFormat = GL_RGBA2; break;
					case 4: mInternalFormat = GL_RGBA4; break;
					case 8: mInternalFormat = GL_RGBA8; break;
					case 12: mInternalFormat = GL_RGBA12; break;
					case 16: mInternalFormat = GL_RGBA16; break;
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
					case 8: mInternalFormat = GL_R8UI; break;
					case 16: mInternalFormat = GL_R16UI; break;
					case 32: mInternalFormat = GL_R32UI; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 2:
					switch (bitSize)
					{
					case 8: mInternalFormat = GL_RG8UI; break;
					case 16: mInternalFormat = GL_RG16UI; break;
					case 32: mInternalFormat = GL_RG32UI; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 3:
					switch (bitSize)
					{
					case 8: mInternalFormat = GL_RGB8UI; break;
					case 16: mInternalFormat = GL_RGB16UI; break;
					case 32: mInternalFormat = GL_RGB32UI; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 4:
					switch (bitSize)
					{
					case 8: mInternalFormat = GL_RGBA8UI; break;
					case 16: mInternalFormat = GL_RGBA16UI; break;
					case 32: mInternalFormat = GL_RGBA32UI; break;
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
					case 8: mInternalFormat = GL_R8_SNORM; break;
					case 16: mInternalFormat = GL_R16_SNORM; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 2:
					switch (bitSize)
					{
					case 8: mInternalFormat = GL_RG8_SNORM; break;
					case 16: mInternalFormat = GL_RG16_SNORM; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 3:
					switch (bitSize)
					{
					case 8: mInternalFormat = GL_RGB8_SNORM; break;
					case 16: mInternalFormat = GL_RGB16_SNORM; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 4:
					switch (bitSize)
					{
					case 8: mInternalFormat = GL_RGBA8_SNORM; break;
					case 16: mInternalFormat = GL_RGBA16_SNORM; break;
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
					case 8: mInternalFormat = GL_R8I; break;
					case 16: mInternalFormat = GL_R16I; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 2:
					switch (bitSize)
					{
					case 8: mInternalFormat = GL_RG8I; break;
					case 16: mInternalFormat = GL_RG16I; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 3:
					switch (bitSize)
					{
					case 8: mInternalFormat = GL_RGB8I; break;
					case 16: mInternalFormat = GL_RGB16I; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 4:
					switch (bitSize)
					{
					case 8: mInternalFormat = GL_RGBA8I; break;
					case 16: mInternalFormat = GL_RGBA16I; break;
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
				case 16: mInternalFormat = GL_R16F; break;
				case 32: mInternalFormat = GL_R32F; break;
				default:
					THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
				}
				break;
			case 2:
				switch (bitSize)
				{
				case 16: mInternalFormat = GL_RG16F; break;
				case 32: mInternalFormat = GL_RG32F; break;
				default:
					THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
				}
				break;
			case 3:
				switch (bitSize)
				{
				case 16: mInternalFormat = GL_RGB16F; break;
				case 32: mInternalFormat = GL_RGB32F; break;
				default:
					THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
				}
				break;
			case 4:
				switch (bitSize)
				{
				case 16: mInternalFormat = GL_RGBA16F; break;
				case 32: mInternalFormat = GL_RGBA32F; break;
				default:
					THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
				}
				break;
			}
			break;

		default:
			// Error, unknown internal type
			THROW_MPP("Unknown texture internal format.", __LINE__, __FILE__, __func__);
		}
	}

	void ProgrammaticTextureStream::setTarget(TextureStream::Target target)
	{
		switch (target)
		{
		case Target::Texture1D:
			mTarget = GL_TEXTURE_1D;
			break;

		case Target::Texture2D:
			mTarget = GL_TEXTURE_2D;
			break;

		case Target::Texture3D:
			mTarget = GL_TEXTURE_3D;
			break;

		case Target::CubeMap:
			mTarget = GL_TEXTURE_CUBE_MAP;
			break;

		default:
			THROW_MPP("Unknown texture target.", __LINE__, __FILE__, __func__);
		}
	}

	void ProgrammaticTextureStream::setData(TextureStream::Target target, ImageLoadFunction creator)
	{
		setTarget(target);

		mQualitySettings[0].source = "";
		mLoadFunc = creator;
	}

	void ProgrammaticTextureStream::setFile(TextureStream::Target target, string const& filename, ImageLoadFunction loader)
	{
		setTarget(target);

		mQualitySettings[0].source = filename;
		mLoadFunc = loader;
	}

	void ProgrammaticTextureStream::setFiltering(Filtering minFilter, Filtering magFilter)
	{
		switch (minFilter)
		{
		case Filtering::Nearest:
			mQualitySettings[0].params.minFilter = GL_NEAREST;
			break;

		case Filtering::Linear:
			mQualitySettings[0].params.minFilter = GL_LINEAR;
			break;

		default:
			THROW_MPP("Unknown texture filter setting.", __LINE__, __FILE__, __func__);
		}

		switch (magFilter)
		{
		case Filtering::Nearest:
			mQualitySettings[0].params.magFilter = GL_NEAREST;
			break;

		case Filtering::Linear:
			mQualitySettings[0].params.magFilter = GL_LINEAR;
			break;

		default:
			THROW_MPP("Unknown texture filter setting.", __LINE__, __FILE__, __func__);
		}
	}

	void ProgrammaticTextureStream::setWrapping(Wrapping wrapping)
	{
		switch (wrapping)
		{
		case Wrapping::Repeat:
			mQualitySettings[0].params.wrap = GL_REPEAT;
			break;

		case Wrapping::MirroredRepeat:
			mQualitySettings[0].params.wrap = GL_MIRRORED_REPEAT;
			break;

		case Wrapping::ClampToEdge:
			mQualitySettings[0].params.wrap = GL_CLAMP_TO_EDGE;
			break;

		case Wrapping::ClampToBorder: 
			mQualitySettings[0].params.wrap = GL_CLAMP_TO_BORDER;
			break;

		default:
			THROW_MPP("Unknown texture wrap setting.", __LINE__, __FILE__, __func__);
		}
	}
}