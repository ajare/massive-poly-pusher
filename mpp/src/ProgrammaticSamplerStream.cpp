#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#	include <Windows.h>
#endif

#include <glew/glew.h>
#include <gl/GL.h>

#include <cassert>

#include "mpp/ProgrammaticSamplerStream.h"
#include "mpp/ResourceManager.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{

	ProgrammaticSamplerStream::ProgrammaticSamplerStream(ResourceManager* resourceMgr)
		: SamplerStream(resourceMgr)
	{
	}

	void ProgrammaticSamplerStream::setFiltering(SamplerParams::MinFilter minFilter, SamplerParams::MagFilter magFilter, uint32_t quality)
	{
		switch (minFilter)
		{
		case SamplerParams::MinFilter::Nearest:
			mQualitySettings[quality].params.minFilter = GL_NEAREST;
			break;

		case SamplerParams::MinFilter::Linear:
			mQualitySettings[quality].params.minFilter = GL_LINEAR;
			break;

		case SamplerParams::MinFilter::NearestMipmapNearest:
			mQualitySettings[quality].params.minFilter = GL_NEAREST_MIPMAP_NEAREST;
			break;

		case SamplerParams::MinFilter::LinearMipmapNearest:
			mQualitySettings[quality].params.minFilter = GL_LINEAR_MIPMAP_NEAREST;
			break;

		case SamplerParams::MinFilter::NearestMipmapLinear:
			mQualitySettings[quality].params.minFilter = GL_NEAREST_MIPMAP_LINEAR;
			break;

		case SamplerParams::MinFilter::LinearMipmapLinear:
			mQualitySettings[quality].params.minFilter = GL_LINEAR_MIPMAP_LINEAR;
			break;

		default:
			THROW_MPP("Unknown texture filter setting.", __LINE__, __FILE__, __func__);
		}

		switch (magFilter)
		{
		case SamplerParams::MagFilter::Nearest:
			mQualitySettings[quality].params.magFilter = GL_NEAREST;
			break;

		case SamplerParams::MagFilter::Linear:
			mQualitySettings[quality].params.magFilter = GL_LINEAR;
			break;

		default:
			THROW_MPP("Unknown texture filter setting.", __LINE__, __FILE__, __func__);
		}
	}

	void ProgrammaticSamplerStream::setWrapping(SamplerParams::Wrapping wrapping, uint32_t quality)
	{
		switch (wrapping)
		{
		case SamplerParams::Wrapping::Repeat:
			mQualitySettings[quality].params.wrap = GL_REPEAT;
			break;

		case SamplerParams::Wrapping::MirroredRepeat:
			mQualitySettings[quality].params.wrap = GL_MIRRORED_REPEAT;
			break;

		case SamplerParams::Wrapping::ClampToEdge:
			mQualitySettings[quality].params.wrap = GL_CLAMP_TO_EDGE;
			break;

		case SamplerParams::Wrapping::ClampToBorder:
			mQualitySettings[quality].params.wrap = GL_CLAMP_TO_BORDER;
			break;

		default:
			THROW_MPP("Unknown texture wrap setting.", __LINE__, __FILE__, __func__);
		}
	}

	void ProgrammaticSamplerStream::setLodBaseLevel(float level, uint32_t quality)
	{
		mQualitySettings[quality].params.lodBaseLevel = level;
	}

	void ProgrammaticSamplerStream::setLodMaxLevel(float level, uint32_t quality)
	{
		mQualitySettings[quality].params.lodMaxLevel = level;
	}

	void ProgrammaticSamplerStream::setLodBias(float bias, uint32_t quality)
	{
		mQualitySettings[quality].params.lodBias = bias;
	}

	void ProgrammaticSamplerStream::setMaxAnisotropy(float maxAnisotropy, uint32_t quality)
	{
		mQualitySettings[quality].params.maxAnisotropy = maxAnisotropy;
	}
}