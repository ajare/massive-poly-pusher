#include <algorithm>
#include <array>
#include <limits>
#include <unordered_set>
#include <utility>

#include "mpp/ParticleEffectValidator.h"

namespace mpp
{
	namespace
	{
		std::string emitterPath(size_t index)
		{
			return "/ParticleEffect/Emitters/Emitter[" + std::to_string(index) + "]";
		}

		void validateCurve(DiagnosticBag& diagnostics, ParticleCurve const& curve,
			std::string const& sourceName, std::string const& path)
		{
			float previous = -1.0f;
			for (auto const& key : curve.keys)
			{
				if (key.time < 0.0f || key.time > 1.0f || key.time < previous)
					diagnostics.error("MPP-PARTICLE-010", "Curve key times must be ordered in [0, 1].", { sourceName, path + "/Keys/Key/time" });
				previous = key.time;
			}
		}

		void validateGradient(DiagnosticBag& diagnostics, ParticleGradient const& gradient,
			std::string const& sourceName, std::string const& path)
		{
			float previous = -1.0f;
			for (auto const& key : gradient.keys)
			{
				if (key.time < 0.0f || key.time > 1.0f || key.time < previous)
					diagnostics.error("MPP-PARTICLE-010", "Gradient key times must be ordered in [0, 1].", { sourceName, path + "/Keys/Key/time" });
				previous = key.time;
			}
		}
	}

	DiagnosticBag ParticleEffectValidator::validate(ParticleEffectSpecification const& specification, std::string const& sourceName)
	{
		DiagnosticBag diagnostics;
		auto error = [&](std::string code, std::string message, std::string path)
		{
			diagnostics.error(std::move(code), std::move(message), { sourceName, std::move(path) });
		};

		if (specification.version != 1)
			error("MPP-PARTICLE-013", "Unsupported particle effect version; expected 1.", "/ParticleEffect/version");

		std::array<std::pair<char const*, ParticleScalarCurve>, 6> const curveNames{{
			{ "Size", ParticleScalarCurve::Size },
			{ "Alpha", ParticleScalarCurve::Alpha },
			{ "VelocityMultiplier", ParticleScalarCurve::VelocityMultiplier },
			{ "Drag", ParticleScalarCurve::Drag },
			{ "RotationSpeed", ParticleScalarCurve::RotationSpeed },
			{ "EmissiveIntensity", ParticleScalarCurve::EmissiveIntensity }
		}};

		for (size_t index = 0; index < specification.emitterTemplates.size(); ++index)
		{
			auto const& authored = specification.emitterTemplates[index];
			auto const& emitter = authored.value;
			auto const& simulation = emitter.simulation;
			auto const path = emitterPath(index);
			auto const spawnPath = path + "/Spawn";

			if (simulation.emissionRateAndPadding[0] < 0.0f ||
				simulation.lifetimeSizeRanges[0] < 0.0f ||
				simulation.lifetimeSizeRanges[1] < simulation.lifetimeSizeRanges[0] ||
				simulation.lifetimeSizeRanges[2] < 0.0f ||
				simulation.lifetimeSizeRanges[3] < simulation.lifetimeSizeRanges[2])
			{
				error("MPP-PARTICLE-011", "Spawn rates and ranges must be non-negative and ordered.", spawnPath);
			}

			auto const modules = simulation.shapeSeedModulesBudget[2];
			if ((modules & uint32_t(ParticleBehaviourModule::Turbulence)) != 0u)
			{
				auto const& values = simulation.turbulenceOctavesLacunarityGain;
				if (values[0] < 1.0f || values[0] > 8.0f || values[1] < 1.0f || values[2] < 0.0f || values[2] > 1.0f)
				{
					error("MPP-PARTICLE-011", "Turbulence octaves must be in [1, 8], lacunarity at least 1, and gain in [0, 1].",
						path + "/Behaviours/Turbulence");
				}
			}

			if ((modules & uint32_t(ParticleBehaviourModule::Collision)) != 0u)
			{
				auto const& values = simulation.collisionParameters;
				if (values[0] < 0.0f || values[1] < 0.0f || values[1] > 1.0f || values[2] < 0.0f || values[3] < 0.0f)
				{
					error("MPP-PARTICLE-011", "Collision restitution, radius scale, and thickness must be non-negative; friction must be in [0, 1].",
						path + "/Behaviours/Collision");
				}
			}

			for (size_t eventIndex = 0; eventIndex < authored.events.size(); ++eventIndex)
			{
				auto const& event = authored.events[eventIndex];
				auto const eventPath = path + "/Events/Event[" + std::to_string(eventIndex) + "]";
				if (event.action == ParticleEventAction::SecondaryParticleBurst && event.count == 0u)
					error("MPP-PARTICLE-015", "A secondary particle burst count must be greater than zero.", eventPath + "/count");
				if (event.age < 0.0f)
					error("MPP-PARTICLE-015", "A particle age event must have a non-negative age.", eventPath + "/age");
			}

			auto const curvePath = path + "/Curves";
			for (auto const& named : curveNames)
				validateCurve(diagnostics, emitter.curves[size_t(named.second)], sourceName, curvePath + "/" + named.first);
			validateGradient(diagnostics, emitter.colourGradient, sourceName, curvePath + "/Colour");

			auto const lightingPath = path + "/Lighting";
			auto const& lighting = emitter.lighting;
			auto const lightingFlags = lighting.flagsAndPadding[0];
			bool const proxy = (lightingFlags & uint32_t(ParticleLightingFlag::ProxyLight)) != 0u;
			bool const injection = (lightingFlags & uint32_t(ParticleLightingFlag::PbrLightInjection)) != 0u;
			bool const volumetric = (lightingFlags & uint32_t(ParticleLightingFlag::VolumetricContribution)) != 0u;
			if (injection && !proxy)
				error("MPP-PARTICLE-018", "Particle light injection requires proxyLight: true.", lightingPath + "/lightInjection");
			if ((proxy || injection || volumetric) && lighting.rangeAndVolumetric[0] <= 0.0f)
				error("MPP-PARTICLE-018", "Enabled particle lighting requires a positive range.", lightingPath + "/range");
			if (lighting.colourAndIntensity[0] < 0.0f || lighting.colourAndIntensity[1] < 0.0f ||
				lighting.colourAndIntensity[2] < 0.0f || lighting.colourAndIntensity[3] < 0.0f ||
				lighting.rangeAndVolumetric[1] < 0.0f)
			{
				error("MPP-PARTICLE-018", "Particle lighting colour and intensities must be non-negative.", lightingPath);
			}

			auto const appearancePath = path + "/Appearance";
			auto const& appearance = emitter.appearance;
			if (appearance.textureAndAtlas[2] == 0u || appearance.textureAndAtlas[3] == 0u ||
				appearance.modes[0] == 0u || appearance.modes[0] > appearance.textureAndAtlas[2] * appearance.textureAndAtlas[3])
			{
				error("MPP-PARTICLE-012", "Atlas dimensions must be positive and contain frameCount.", appearancePath);
			}
			if (appearance.culling[0] < 0.0f || appearance.culling[1] < 0.0f)
				error("MPP-PARTICLE-011", "Particle culling distances and sizes must be non-negative.", appearancePath);
			if (appearance.culling[3] < 0.0f)
				error("MPP-PARTICLE-011", "Particle distortion strength must be non-negative.", appearancePath);
		}

		if (specification.emitterTemplates.empty() && specification.childEffects.empty())
			error("MPP-PARTICLE-003", "Particle effect requires at least one emitter template or child particle effect.", "/ParticleEffect");

		bool const hasSecondaryEvents = std::any_of(specification.emitterTemplates.begin(),
			specification.emitterTemplates.end(), [](auto const& emitter)
			{
				return std::any_of(emitter.events.begin(), emitter.events.end(), [](auto const& event)
					{ return event.action == ParticleEventAction::SecondaryParticleBurst; });
			});
		std::unordered_set<std::string> emitterNames;
		for (size_t index = 0; index < specification.emitterTemplates.size(); ++index)
		{
			auto const& emitter = specification.emitterTemplates[index];
			auto const path = emitterPath(index);
			if (!emitterNames.emplace(emitter.name).second && hasSecondaryEvents)
				error("MPP-PARTICLE-016", "Emitter template names must be unique when secondary particle events are authored.", path + "/name");
			for (size_t eventIndex = 0; eventIndex < emitter.events.size(); ++eventIndex)
			{
				auto const& event = emitter.events[eventIndex];
				if (event.action != ParticleEventAction::SecondaryParticleBurst) continue;
				auto const found = std::find_if(specification.emitterTemplates.begin(), specification.emitterTemplates.end(),
					[&](auto const& candidate) { return candidate.name == event.targetEmitter; });
				if (found == specification.emitterTemplates.end())
				{
					error("MPP-PARTICLE-017", "Secondary particle event target emitter '" + event.targetEmitter + "' does not exist.",
						path + "/Events/Event[" + std::to_string(eventIndex) + "]/targetEmitter");
				}
			}
		}

		uint64_t total = 0;
		for (auto const& emitter : specification.emitterTemplates)
			total += emitter.value.simulation.shapeSeedModulesBudget[3];
		if (total > std::numeric_limits<uint32_t>::max() || total != specification.maximumParticleCount)
		{
			error("MPP-PARTICLE-014", "maximumParticleCount must equal the sum of emitter-template budgets (" + std::to_string(total) + ").",
				"/ParticleEffect/maximumParticleCount");
		}

		return diagnostics;
	}
}
