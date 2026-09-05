#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "mpp/ParticleSystem.h"

namespace mpp
{
	// Authored, reusable particle effect data. It deliberately contains no live
	// emitter handles or instance transforms; those belong to ParticleSystem.
	struct _MPPAPI ParticleEffectSpecification
	{
		struct EventRule
		{
			ParticleEventTrigger trigger{ ParticleEventTrigger::Spawn };
			ParticleEventAction action{ ParticleEventAction::SecondaryParticleBurst };
			// Authored emitter-template name; required only for secondary bursts.
			std::string targetEmitter;
			uint32_t count{ 1 };
			float age{ 0.0f };
			uint32_t payload{ 0 };
		};

		struct EmitterTemplate
		{
			std::string name;
			ParticleEmitterTemplate value;
			// Optional ResourceManager name for the appearance's albedo/flipbook.
			std::string albedoTexture;
			// A model selects mesh-particle rendering. material is an optional override;
			// when empty, each model mesh keeps its embedded material.
			std::string meshModel;
			std::string meshMaterial;
			std::vector<EventRule> events;
		};

		uint32_t version{ 1 };
		std::string name;
		uint32_t maximumParticleCount{ 0 };
		std::vector<EmitterTemplate> emitterTemplates;
	};
}
