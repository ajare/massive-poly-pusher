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
			auto resolve = [&](std::string const& name, ResourcePtr& destination)
			{
				if (name.empty()) return;
				auto resource = getResourceManager()->getResource(name);
				destination = resource;
				acquireDependentResource(resource);
			};
			resolve(authored[index].albedoTexture, mEmitterTemplates[index].albedoTexture);
			resolve(authored[index].meshModel, mEmitterTemplates[index].meshModel);
			resolve(authored[index].meshMaterial, mEmitterTemplates[index].meshMaterial);
		}
		invalidateCurveLut();
	}

	void ParticleEffect::destroyImpl()
	{
		mEmitterTemplates.clear();
		invalidateCurveLut();
	}
}
