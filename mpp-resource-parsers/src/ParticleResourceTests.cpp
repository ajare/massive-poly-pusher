#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "mpp/ParticleData.h"
#include "mpp/ParticleSystem.h"
#include "mpp/resource-parsers/ParticleResourceTests.h"

namespace mpp::resource_parsers
{
	bool runParticleResourceTests(std::string* failure)
	{
		auto fail = [failure](std::string const& message)
		{
			if (failure) *failure = message;
			return false;
		};

		// These values are the authored wire values used by particle resources.
		// Pinning them here prevents adding or reordering an enum from silently
		// changing an existing document's meaning.
		std::array<uint32_t, 7> const shapes{
			uint32_t(ParticleSpawnShape::Point), uint32_t(ParticleSpawnShape::Line),
			uint32_t(ParticleSpawnShape::Box), uint32_t(ParticleSpawnShape::Sphere),
			uint32_t(ParticleSpawnShape::Hemisphere), uint32_t(ParticleSpawnShape::Disc),
			uint32_t(ParticleSpawnShape::Cone)
		};
		for (size_t index = 0; index < shapes.size(); ++index)
			if (shapes[index] != index) return fail("particle spawn-shape wire values changed");

		auto const modules = uint32_t(ParticleBehaviourModule::Gravity) |
			uint32_t(ParticleBehaviourModule::Drag) | uint32_t(ParticleBehaviourModule::Noise);
		if (modules != 0x7u) return fail("particle behaviour-module flags overlap or changed");
		if (uint32_t(ParticleTextureAnimation::FrameOverLife | ParticleTextureAnimation::RandomStart) != 0x101u)
			return fail("particle texture-animation flags are no longer composable");

		ParticleEmitterTemplate emitter;
		if (emitter.simulation.emissionState[1] != 1u ||
			emitter.simulation.parameterMultipliers0 != std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f } ||
			emitter.appearance.textureAndAtlas[2] != 1u || emitter.appearance.textureAndAtlas[3] != 1u ||
			emitter.appearance.modes[0] != 1u || emitter.appearance.modes[2] != uint32_t(ParticleBillboardMode::CameraFacing) ||
			emitter.appearance.modes[3] != uint32_t(ParticleBlendClass::Additive))
			return fail("particle emitter-template resource defaults changed");

		// Resource construction must copy templates. A reusable particle effect
		// cannot share mutable emitter data between live instances.
		auto copy = emitter;
		copy.simulation.emissionRateAndPadding[0] = 42.0f;
		copy.appearance.tintAndAlpha[0] = 0.25f;
		if (emitter.simulation.emissionRateAndPadding[0] != 0.0f || emitter.appearance.tintAndAlpha[0] != 1.0f)
			return fail("particle emitter-template copies share mutable state");

		return true;
	}
}
