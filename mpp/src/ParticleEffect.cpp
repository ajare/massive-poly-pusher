#include <utility>

#include "mpp/ParticleEffect.h"
#include "mpp/ParticleEffectStream.h"
#include "mpp/ResourceManager.h"
#include "mpp/MppException.h"

namespace mpp
{
	ParticleEffect::ParticleEffect(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceManager, ResourceStreamPtr stream)
		: Resource(name, "ParticleEffect", renderSystem, resourceManager, std::move(stream))
	{
	}

	void ParticleEffect::createImpl()
	{
		auto stream = dynamic_cast<ParticleEffectStream*>(getResourceStream().get());
		if (!stream) THROW_MPP("ParticleEffect resource requires a ParticleEffectStream.", __LINE__, __FILE__, __func__);
		mEmitterTemplates.assign(stream->getEmitterTemplates().begin(), stream->getEmitterTemplates().end());
		auto const& authored = stream->getSpecification().emitterTemplates;
		for (size_t index = 0; index < authored.size(); ++index)
		{
			if (authored[index].albedoTexture.empty()) continue;
			auto texture = getResourceManager()->getResource(authored[index].albedoTexture);
			mEmitterTemplates[index].albedoTexture = texture;
			acquireDependentResource(texture);
		}
		invalidateCurveLut();
	}

	void ParticleEffect::destroyImpl()
	{
		mEmitterTemplates.clear();
		invalidateCurveLut();
	}
}
