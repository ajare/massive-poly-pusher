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

	void ProgrammaticTextureStream::setData(TextureStream::Target target, ImageLoadFunction creator, uint32_t quality)
	{
		setTarget(target);

		mQualitySettings[quality].source = "";
		mQualitySettings[quality].loadFunc = creator;
	}

	void ProgrammaticTextureStream::setFile(TextureStream::Target target, string const& filename, ImageLoadFunction loader, uint32_t quality)
	{
		setTarget(target);

		mQualitySettings[quality].source = filename;
		mQualitySettings[quality].loadFunc = loader;
	}

	void ProgrammaticTextureStream::setFiltering(TextureParams::MinFilter minFilter, TextureParams::MagFilter magFilter, uint32_t quality)
	{
		switch (minFilter)
		{
		case TextureParams::MinFilter::Nearest:
			mQualitySettings[quality].params.minFilter = GL_NEAREST;
			break;

		case TextureParams::MinFilter::Linear:
			mQualitySettings[quality].params.minFilter = GL_LINEAR;
			break;

		case TextureParams::MinFilter::NearestMipmapNearest:
			mQualitySettings[quality].params.minFilter = GL_NEAREST_MIPMAP_NEAREST;
			enableMipMaps(true);
			break;

		case TextureParams::MinFilter::LinearMipmapNearest:
			mQualitySettings[quality].params.minFilter = GL_LINEAR_MIPMAP_NEAREST;
			enableMipMaps(true);
			break;

		case TextureParams::MinFilter::NearestMipmapLinear:
			mQualitySettings[quality].params.minFilter = GL_NEAREST_MIPMAP_LINEAR;
			enableMipMaps(true);
			break;

		case TextureParams::MinFilter::LinearMipmapLinear:
			mQualitySettings[quality].params.minFilter = GL_LINEAR_MIPMAP_LINEAR;
			enableMipMaps(true);
			break;

		default:
			THROW_MPP("Unknown texture filter setting.", __LINE__, __FILE__, __func__);
		}

		switch (magFilter)
		{
		case TextureParams::MagFilter::Nearest:
			mQualitySettings[quality].params.magFilter = GL_NEAREST;
			break;

		case TextureParams::MagFilter::Linear:
			mQualitySettings[quality].params.magFilter = GL_LINEAR;
			break;

		default:
			THROW_MPP("Unknown texture filter setting.", __LINE__, __FILE__, __func__);
		}
	}

	void ProgrammaticTextureStream::setWrapping(TextureParams::Wrapping wrapping, uint32_t quality)
	{
		switch (wrapping)
		{
		case TextureParams::Wrapping::Repeat:
			mQualitySettings[quality].params.wrap = GL_REPEAT;
			break;

		case TextureParams::Wrapping::MirroredRepeat:
			mQualitySettings[quality].params.wrap = GL_MIRRORED_REPEAT;
			break;

		case TextureParams::Wrapping::ClampToEdge:
			mQualitySettings[quality].params.wrap = GL_CLAMP_TO_EDGE;
			break;

		case TextureParams::Wrapping::ClampToBorder:
			mQualitySettings[quality].params.wrap = GL_CLAMP_TO_BORDER;
			break;

		default:
			THROW_MPP("Unknown texture wrap setting.", __LINE__, __FILE__, __func__);
		}
	}

	void ProgrammaticTextureStream::enableMipMaps(bool enable, uint32_t quality)
	{
		mQualitySettings[quality].params.useMipmaps = enable;
	}

	void ProgrammaticTextureStream::setLodBaseLevel(float level, uint32_t quality)
	{
		mQualitySettings[quality].params.lodBaseLevel = level;
	}

	void ProgrammaticTextureStream::setLodMaxLevel(float level, uint32_t quality)
	{
		mQualitySettings[quality].params.lodMaxLevel = level;
	}

	void ProgrammaticTextureStream::setLodBias(float bias, uint32_t quality)
	{
		mQualitySettings[quality].params.lodBias = bias;
	}

	void ProgrammaticTextureStream::setMaxAnisotropy(float maxAnisotropy, uint32_t quality)
	{
		mQualitySettings[quality].params.maxAnisotropy = maxAnisotropy;
	}

	void ProgrammaticTextureStream::setSampler(string const& sampler, uint32_t quality)
	{
		mQualitySettings[quality].sampler = sampler;
	}
}