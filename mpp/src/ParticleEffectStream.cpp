#include "mpp/ParticleEffectStream.h"

namespace mpp
{
	ParticleEffectStream::ParticleEffectStream(ResourceManager* resourceManager)
		: ResourceStream(resourceManager, "ParticleEffect")
	{
	}

	void ParticleEffectStream::rebuildEmitterTemplates()
	{
		mEmitterTemplates.clear();
		mEmitterTemplates.reserve(mSpecification.emitterTemplates.size());
		for (auto const& emitter : mSpecification.emitterTemplates) mEmitterTemplates.push_back(emitter.value);
		invalidateCurveLut();
	}
}
