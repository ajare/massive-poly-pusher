#pragma once

#include "mpp/ParticleEffectStream.h"

namespace mpp
{
	class _MPPAPI ProgrammaticParticleEffectStream : public ParticleEffectStream
	{
		void loadImpl() override {}

	public:
		explicit ProgrammaticParticleEffectStream(ResourceManager* resourceManager);
		void setSpecification(ParticleEffectSpecification const& specification);
		void setName(std::string name);
		void setMaximumParticleCount(uint32_t count);
		void addEmitterTemplate(ParticleEffectSpecification::EmitterTemplate const& emitterTemplate);
	};
}
