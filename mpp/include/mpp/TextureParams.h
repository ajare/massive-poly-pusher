#pragma once

#include "Config.h"

namespace mpp
{
	struct _MPPAPI TextureParams
	{
		enum class MinFilter
		{
			Nearest,
			Linear,
			NearestMipmapNearest,
			LinearMipmapNearest,
			NearestMipmapLinear,
			LinearMipmapLinear
		};

		enum class MagFilter
		{
			Nearest,
			Linear
		};

		enum class Wrapping
		{
			Repeat,
			MirroredRepeat,
			ClampToEdge,
			ClampToBorder
		};

	public:

		uint32_t minFilter;
		uint32_t magFilter;
		uint32_t wrap;

		bool useMipmaps;
		int32_t lodBaseLevel, lodMaxLevel;
		float lodBias;
		float maxAnisotropy;

	public:

		TextureParams();
	};
}