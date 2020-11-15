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

	void ProgrammaticSamplerStream::setFiltering(SamplerParams::MinFilter minFilter, SamplerParams::MagFilter magFilter)
	{
		switch (minFilter)
		{
		case SamplerParams::MinFilter::Nearest:
			mQualitySettings[0].params.minFilter = GL_NEAREST;
			break;

		case SamplerParams::MinFilter::Linear:
			mQualitySettings[0].params.minFilter = GL_LINEAR;
			break;

		case SamplerParams::MinFilter::NearestMipmapNearest:
			mQualitySettings[0].params.minFilter = GL_NEAREST_MIPMAP_NEAREST;
			break;

		case SamplerParams::MinFilter::LinearMipmapNearest:
			mQualitySettings[0].params.minFilter = GL_LINEAR_MIPMAP_NEAREST;
			break;

		case SamplerParams::MinFilter::NearestMipmapLinear:
			mQualitySettings[0].params.minFilter = GL_NEAREST_MIPMAP_LINEAR;
			break;

		case SamplerParams::MinFilter::LinearMipmapLinear:
			mQualitySettings[0].params.minFilter = GL_LINEAR_MIPMAP_LINEAR;
			break;

		default:
			THROW_MPP("Unknown texture filter setting.", __LINE__, __FILE__, __func__);
		}

		switch (magFilter)
		{
		case SamplerParams::MagFilter::Nearest:
			mQualitySettings[0].params.magFilter = GL_NEAREST;
			break;

		case SamplerParams::MagFilter::Linear:
			mQualitySettings[0].params.magFilter = GL_LINEAR;
			break;

		default:
			THROW_MPP("Unknown texture filter setting.", __LINE__, __FILE__, __func__);
		}
	}

	void ProgrammaticSamplerStream::setWrapping(SamplerParams::Wrapping wrapping)
	{
		switch (wrapping)
		{
		case SamplerParams::Wrapping::Repeat:
			mQualitySettings[0].params.wrap = GL_REPEAT;
			break;

		case SamplerParams::Wrapping::MirroredRepeat:
			mQualitySettings[0].params.wrap = GL_MIRRORED_REPEAT;
			break;

		case SamplerParams::Wrapping::ClampToEdge:
			mQualitySettings[0].params.wrap = GL_CLAMP_TO_EDGE;
			break;

		case SamplerParams::Wrapping::ClampToBorder:
			mQualitySettings[0].params.wrap = GL_CLAMP_TO_BORDER;
			break;

		default:
			THROW_MPP("Unknown texture wrap setting.", __LINE__, __FILE__, __func__);
		}
	}

	void ProgrammaticSamplerStream::setLodBaseLevel(float level)
	{
		mQualitySettings[0].params.lodBaseLevel = level;
	}

	void ProgrammaticSamplerStream::setLodMaxLevel(float level)
	{
		mQualitySettings[0].params.lodMaxLevel = level;
	}

	void ProgrammaticSamplerStream::setLodBias(float bias)
	{
		mQualitySettings[0].params.lodBias = bias;
	}
}