#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#include "mpp/data/StructuredData.h"
#include "mpp/ParticleData.h"
#include "mpp/ParticleEffectSpecification.h"
#include "mpp/ProgrammaticParticleEffectStream.h"
#include "mpp/resource-parsers/FileParticleEffectStream.h"
#include "mpp/resource-parsers/ParticleEffectParser.h"
#include "mpp/resource-parsers/ParticleEffectSerializer.h"
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
		std::array<uint32_t, 6> const billboards{
			uint32_t(ParticleBillboardMode::CameraFacing), uint32_t(ParticleBillboardMode::ScreenAligned),
			uint32_t(ParticleBillboardMode::Cylindrical), uint32_t(ParticleBillboardMode::AxisLocked),
			uint32_t(ParticleBillboardMode::VelocityAligned), uint32_t(ParticleBillboardMode::VelocityStretched)
		};
		for (size_t index = 0; index < billboards.size(); ++index)
			if (billboards[index] != index) return fail("particle billboard-mode wire values changed");

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

		ParticleEffectSpecification specification;
		specification.name = "RoundTrip";
		specification.maximumParticleCount = 96;
		ParticleEffectSpecification::EmitterTemplate authored;
		authored.name = "Smoke";
		authored.albedoTexture = "Textures/Smoke";
		authored.value.localTransform[3][0] = 2.0f;
		auto& simulation = authored.value.simulation;
		simulation.shapeSeedModulesBudget = { uint32_t(ParticleSpawnShape::Cone), 77u,
			uint32_t(ParticleBehaviourModule::Gravity) | uint32_t(ParticleBehaviourModule::Drag), 96u };
		simulation.shapeParameters = { 1.0f, 2.0f, 0.5f, 0.0f };
		simulation.emissionRateAndPadding[0] = 24.0f;
		simulation.gravityAndDrag = { 0.0f, -9.81f, 0.0f, 0.2f };
		authored.value.appearance.textureAndAtlas = { 0u, 0u, 4u, 4u };
		authored.value.appearance.modes = { 16u, uint32_t(ParticleTextureAnimation::FrameOverLife) | ParticleTextureRandomStartBit,
			uint32_t(ParticleBillboardMode::VelocityStretched), uint32_t(ParticleBlendClass::Alpha) };
		authored.value.curves[size_t(ParticleScalarCurve::Size)].keys = { { 0.0f, 0.25f }, { 1.0f, 2.0f } };
		authored.value.colourGradient.keys = { { 0.0f, { 1.0f, 0.5f, 0.1f } }, { 1.0f, { 0.1f, 0.2f, 0.3f } } };
		specification.emitterTemplates.push_back(authored);

		auto temporary = std::filesystem::temp_directory_path() / "mpp-particle-resource-round-trip.particle.yaml";
		try { ParticleEffectSerializer::toFile(specification, temporary.string()); }
		catch (std::exception const& error) { return fail("could not write particle effect round trip: " + std::string(error.what())); }
		auto parsed = ParticleEffectParser::fromFile(temporary.string());
		std::error_code ignored; std::filesystem::remove(temporary, ignored);
		if (!parsed.succeeded()) return fail("serialized particle effect did not parse without diagnostics");
		auto const& restored = parsed.specification;
		if (restored.name != specification.name || restored.maximumParticleCount != 96 || restored.emitterTemplates.size() != 1 ||
			restored.emitterTemplates[0].name != authored.name || restored.emitterTemplates[0].albedoTexture != authored.albedoTexture ||
			restored.emitterTemplates[0].value.simulation.shapeSeedModulesBudget != simulation.shapeSeedModulesBudget ||
			restored.emitterTemplates[0].value.appearance.modes != authored.value.appearance.modes ||
			restored.emitterTemplates[0].value.localTransform[3][0] != 2.0f ||
			restored.emitterTemplates[0].value.curves[size_t(ParticleScalarCurve::Size)].keys.size() != 2 ||
			restored.emitterTemplates[0].value.colourGradient.keys.size() != 2)
			return fail("particle effect did not round-trip through *.particle.yaml");

		// The C++ stream consumes the same plain specification without creating a
		// GL object or a live emitter.
		ProgrammaticParticleEffectStream programmatic(nullptr);
		programmatic.setSpecification(specification);
		if (programmatic.getSpecification().maximumParticleCount != restored.maximumParticleCount ||
			programmatic.getEmitterTemplates().size() != restored.emitterTemplates.size() ||
			programmatic.getEmitterTemplates()[0].simulation.shapeSeedModulesBudget != restored.emitterTemplates[0].value.simulation.shapeSeedModulesBudget)
			return fail("programmatic particle effect stream was not equivalent to parsed authoring");

		// Ordered behaviour lists are intentionally outside the schema, and a bad
		// effect-level budget is an authoring diagnostic. Neither case may throw.
		mpp::data::StructuredData malformed("ParticleEffect");
		malformed.addEntry("name", "Bad"); malformed.addEntry("maximumParticleCount", "4");
		mpp::data::StructuredData emitters("Emitters"), badEmitter("Emitter"), spawn("Spawn"), behaviours("Behaviours");
		badEmitter.addEntry("name", "BadEmitter"); badEmitter.addEntry("maximumParticleCount", "3"); spawn.addEntry("shape", "point"); badEmitter.addEntry("Spawn", spawn);
		behaviours.addEntry("Module", "gravity"); badEmitter.addEntry("Behaviours", behaviours); emitters.addEntry("Emitter", badEmitter); malformed.addEntry("Emitters", emitters);
		auto invalid = ParticleEffectParser::fromData(malformed, "malformed.particle.yaml");
		if (!invalid.diagnostics.hasErrors() || invalid.diagnostics.count(DiagnosticSeverity::Error) < 2)
			return fail("malformed particle effect did not return behaviour-schema and budget diagnostics");
		try
		{
			FileParticleEffectStream malformedStream(nullptr, "malformed.particle.yaml", malformed);
			malformedStream.load();
			if (!malformedStream.getDiagnostics().hasErrors()) return fail("file particle stream discarded malformed-document diagnostics");
			FileParticleEffectStream unreadableStream(nullptr, "missing.particle.yaml");
			unreadableStream.load();
			if (!unreadableStream.getDiagnostics().hasErrors()) return fail("unreadable particle document did not produce diagnostics");
		}
		catch (...) { return fail("malformed particle file stream threw instead of returning diagnostics"); }

		return true;
	}
}
