#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#	include <Windows.h>
#endif

#include <glew/glew.h>
#include <gl/GL.h>

#include "mpp/ProgrammaticRenderTextureStream.h"
#include "mpp/ResourceManager.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{

	ProgrammaticRenderTextureStream::ProgrammaticRenderTextureStream(ResourceManager* resourceMgr)
		: RenderTextureStream(resourceMgr)
	{
		createQualitySetting("");
	}

	void ProgrammaticRenderTextureStream::setWidth(size_t width, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.width = width;
	}

	void ProgrammaticRenderTextureStream::setHeight(size_t height, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.height = height;
	}

	void ProgrammaticRenderTextureStream::setDepth(size_t depth, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.depth = depth;
	}

	void ProgrammaticRenderTextureStream::setParams(TextureParams const& params, uint32_t quality)
	{
		mQualitySettings[quality].params = params;
	}

	void ProgrammaticRenderTextureStream::setInternalFormat(TextureInternalType type, bool normalized, size_t bitSize, size_t channels, uint32_t quality)
	{
		if (type == TextureInternalType::Float && normalized)
		{
			getResourceMgr()->warnMessage("ProgrammaticTextureStream: ignoring 'normalized': specified with floating-point data.");
		}

		if (channels < 1 || channels > 4)
		{
			// Error
			THROW_MPP("Invalid texture channel count.", __LINE__, __FILE__, __func__);
		}

		uint32_t internalFormat{ 0 };
		switch (type)
		{
		case TextureInternalType::UnsignedInteger:
			if (normalized)
			{
				switch (channels)
				{
				case 1:
					switch (bitSize)
					{
					case 8: internalFormat = GL_R8; break;
					case 16: internalFormat = GL_R16; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 2:
					switch (bitSize)
					{
					case 8: internalFormat = GL_RG8; break;
					case 16: internalFormat = GL_RG16; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 3:
					switch (bitSize)
					{
					case 4: internalFormat = GL_RGB4; break;
					case 5: internalFormat = GL_RGB5; break;
					case 8: internalFormat = GL_RGB8; break;
					case 10: internalFormat = GL_RGB10; break;
					case 12: internalFormat = GL_RGB12; break;
					case 16: internalFormat = GL_RGB16; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 4:
					switch (bitSize)
					{
					case 2: internalFormat = GL_RGBA2; break;
					case 4: internalFormat = GL_RGBA4; break;
					case 8: internalFormat = GL_RGBA8; break;
					case 12: internalFormat = GL_RGBA12; break;
					case 16: internalFormat = GL_RGBA16; break;
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
					case 8: internalFormat = GL_R8UI; break;
					case 16: internalFormat = GL_R16UI; break;
					case 32: internalFormat = GL_R32UI; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 2:
					switch (bitSize)
					{
					case 8: internalFormat = GL_RG8UI; break;
					case 16: internalFormat = GL_RG16UI; break;
					case 32: internalFormat = GL_RG32UI; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 3:
					switch (bitSize)
					{
					case 8: internalFormat = GL_RGB8UI; break;
					case 16: internalFormat = GL_RGB16UI; break;
					case 32: internalFormat = GL_RGB32UI; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 4:
					switch (bitSize)
					{
					case 8: internalFormat = GL_RGBA8UI; break;
					case 16: internalFormat = GL_RGBA16UI; break;
					case 32: internalFormat = GL_RGBA32UI; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				}
			}
			break;

		case TextureInternalType::SignedInteger:
			if (normalized)
			{
				switch (channels)
				{
				case 1:
					switch (bitSize)
					{
					case 8: internalFormat = GL_R8_SNORM; break;
					case 16: internalFormat = GL_R16_SNORM; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 2:
					switch (bitSize)
					{
					case 8: internalFormat = GL_RG8_SNORM; break;
					case 16: internalFormat = GL_RG16_SNORM; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 3:
					switch (bitSize)
					{
					case 8: internalFormat = GL_RGB8_SNORM; break;
					case 16: internalFormat = GL_RGB16_SNORM; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 4:
					switch (bitSize)
					{
					case 8: internalFormat = GL_RGBA8_SNORM; break;
					case 16: internalFormat = GL_RGBA16_SNORM; break;
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
					case 8: internalFormat = GL_R8I; break;
					case 16: internalFormat = GL_R16I; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 2:
					switch (bitSize)
					{
					case 8: internalFormat = GL_RG8I; break;
					case 16: internalFormat = GL_RG16I; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 3:
					switch (bitSize)
					{
					case 8: internalFormat = GL_RGB8I; break;
					case 16: internalFormat = GL_RGB16I; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				case 4:
					switch (bitSize)
					{
					case 8: internalFormat = GL_RGBA8I; break;
					case 16: internalFormat = GL_RGBA16I; break;
					default:
						THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
					}
					break;
				}
			}
			break;

		case TextureInternalType::Float:
			switch (channels)
			{
			case 1:
				switch (bitSize)
				{
				case 16: internalFormat = GL_R16F; break;
				case 32: internalFormat = GL_R32F; break;
				default:
					THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
				}
				break;
			case 2:
				switch (bitSize)
				{
				case 16: internalFormat = GL_RG16F; break;
				case 32: internalFormat = GL_RG32F; break;
				default:
					THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
				}
				break;
			case 3:
				switch (bitSize)
				{
				case 16: internalFormat = GL_RGB16F; break;
				case 32: internalFormat = GL_RGB32F; break;
				default:
					THROW_MPP("Invalid bitsize.", __LINE__, __FILE__, __func__);
				}
				break;
			case 4:
				switch (bitSize)
				{
				case 16: internalFormat = GL_RGBA16F; break;
				case 32: internalFormat = GL_RGBA32F; break;
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

		auto& qs = mQualitySettings[quality];
		qs.internalFormat = internalFormat;
	}

	void ProgrammaticRenderTextureStream::setTarget(TextureTarget target, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		switch (target)
		{
		case TextureTarget::Texture1D:
			qs.target = GL_TEXTURE_1D;
			break;

		case TextureTarget::Texture2D:
			qs.target = GL_TEXTURE_2D;
			break;

		case TextureTarget::Texture3D:
			qs.target = GL_TEXTURE_3D;
			break;

		case TextureTarget::CubeMap:
			qs.target = GL_TEXTURE_CUBE_MAP;
			break;

		default:
			THROW_MPP("Unknown texture target.", __LINE__, __FILE__, __func__);
		}
	}

	void ProgrammaticRenderTextureStream::setFiltering(TextureParams::MinFilter minFilter, TextureParams::MagFilter magFilter, uint32_t quality)
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

	void ProgrammaticRenderTextureStream::setWrapping(TextureParams::Wrapping wrapping, uint32_t quality)
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

	void ProgrammaticRenderTextureStream::enableMipMaps(bool enable, uint32_t quality)
	{
		mQualitySettings[quality].params.useMipmaps = enable;
	}

	void ProgrammaticRenderTextureStream::setLodBaseLevel(int32_t level, uint32_t quality)
	{
		mQualitySettings[quality].params.lodBaseLevel = level;
	}

	void ProgrammaticRenderTextureStream::setLodMaxLevel(int32_t level, uint32_t quality)
	{
		mQualitySettings[quality].params.lodMaxLevel = level;
	}

	void ProgrammaticRenderTextureStream::setLodBias(float bias, uint32_t quality)
	{
		mQualitySettings[quality].params.lodBias = bias;
	}

	void ProgrammaticRenderTextureStream::setMaxAnisotropy(float maxAnisotropy, uint32_t quality)
	{
		mQualitySettings[quality].params.maxAnisotropy = maxAnisotropy;
	}

	void ProgrammaticRenderTextureStream::setSampler(string const& sampler, uint32_t quality)
	{
		mQualitySettings[quality].sampler = sampler;
	}

	void ProgrammaticRenderTextureStream::setDepthBuffer(bool use)
	{
		mUseDepthBuffer = use;
	}

	void ProgrammaticRenderTextureStream::setNumAttachments(size_t numAttachments)
	{
		mNumAttachments = numAttachments;
	}
}