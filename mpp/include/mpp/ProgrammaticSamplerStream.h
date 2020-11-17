#pragma once

#include "mpp/SamplerStream.h"

namespace mpp
{
	class _MPPAPI ProgrammaticSamplerStream : public SamplerStream
	{
	public:

		explicit ProgrammaticSamplerStream(ResourceManager* resourceMgr);

		void setFiltering(SamplerParams::MinFilter minFilter, SamplerParams::MagFilter magFilter, uint32_t quality = 0);

		void setWrapping(SamplerParams::Wrapping wrapping, uint32_t quality = 0);

		void setLodMinLevel(float level, uint32_t quality = 0);

		void setLodMaxLevel(float level, uint32_t quality = 0);

		void setLodBias(float bias, uint32_t quality = 0);

		void setMaxAnisotropy(float maxAnisotropy, uint32_t quality = 0);
	};
}