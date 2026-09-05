#include <array>
#include <cmath>
#include <string>

#include <glm/gtc/matrix_transform.hpp>

#include "mpp/ParticleCurveLut.h"
#include "mpp/ParticleSystem.h"
#include "mpp/ParticleSystemTests.h"

using namespace std;

namespace mpp
{
	bool runParticleSystemCpuTests(string* failure)
	{
		auto fail = [failure](string const& message)
		{
			if (failure) *failure = message;
			return false;
		};
		auto step = [](ParticleSystem& system, float dt)
		{
			system.mSimulationSeconds += dt;
			system.buildSpawnCommands(dt);
			system.mSpawnCommands.clear();
			system.retireCompletedEmitters();
		};
		auto burst = [](float lifetime)
		{
			ParticleEmitterTemplate value;
			value.simulation.emissionState = { 1u, 1u, 1u, 0u };
			value.simulation.lifetimeSizeRanges[0] = lifetime;
			value.simulation.lifetimeSizeRanges[1] = lifetime;
			return value;
		};

		ParticleEmitterTemplate curved;
		curved.curves[size_t(ParticleScalarCurve::Size)].keys = { { 0.0f, 0.5f }, { 1.0f, 2.0f } };
		curved.curves[size_t(ParticleScalarCurve::EmissiveIntensity)].keys = { { 0.0f, 1.0f }, { 1.0f, 6.0f } };
		curved.colourGradient.keys = { { 0.0f, { 1.0f, 0.0f, 0.0f } }, { 1.0f, { 0.0f, 0.5f, 3.0f } } };
		array<ParticleEmitterTemplate, 2> lutTemplates{ curved, ParticleEmitterTemplate{} };
		auto lut = ParticleEffectCurveLut::bake(lutTemplates);
		if (!lut || lut->getWidth() != ParticleEffectCurveLut::SampleCount ||
			lut->getHeight() != ParticleEffectCurveLut::RowsPerTemplate * lutTemplates.size())
			return fail("particle effect LUT dimensions did not partition rows by emitter template");
		if (lut->getRowOffset(0) != 0u || lut->getRowOffset(1) != ParticleEffectCurveLut::RowsPerTemplate)
			return fail("particle effect LUT row offsets were allocated at runtime instead of baked by template order");
		auto const& texels = lut->getFloatTexels();
		auto sample = [&](uint32_t x, uint32_t row, uint32_t channel)
			{ return texels[(size_t(row) * ParticleEffectCurveLut::SampleCount + x) * 4u + channel]; };
		if (abs(sample(ParticleEffectCurveLut::SampleCount - 1u, 0u, 0u) - 2.0f) > 0.0001f ||
			abs(sample(ParticleEffectCurveLut::SampleCount - 1u, 1u, 1u) - 6.0f) > 0.0001f ||
			abs(sample(ParticleEffectCurveLut::SampleCount - 1u, 2u, 2u) - 3.0f) > 0.0001f)
			return fail("size, emissive, or HDR colour values did not survive the particle effect LUT bake");

		auto const randomOverLife = uint32_t(ParticleTextureAnimation::FrameOverLife | ParticleTextureAnimation::RandomStart);
		auto const randomFixedRate = uint32_t(ParticleTextureAnimation::FixedRate | ParticleTextureAnimation::RandomStart);
		if (particleFlipbookFrame(8u, uint32_t(ParticleTextureAnimation::FrameOverLife), 0.5f, 2.0f, 0.0f, 0u) != 2u ||
			particleFlipbookFrame(8u, uint32_t(ParticleTextureAnimation::FixedRate), 1.25f, 2.0f, 4.0f, 0u) != 5u ||
			particleFlipbookFrame(8u, randomOverLife, 0.5f, 2.0f, 0.0f, 11u) != 5u ||
			particleFlipbookFrame(8u, randomFixedRate, 1.25f, 2.0f, 4.0f, 11u) != 0u)
			return fail("flipbook playback or combinable random start selected the wrong frame");

		class TestParticleEffect final : public ParticleEffectSource
		{
			array<ParticleEmitterTemplate, 2> mTemplates;
		public:
			explicit TestParticleEffect(array<ParticleEmitterTemplate, 2> templates) : mTemplates(std::move(templates)) {}
			span<ParticleEmitterTemplate const> getEmitterTemplates() const override { return mTemplates; }
		};
		ParticleSystem system(nullptr, nullptr);
		weak_ptr<ParticleEffectCurveLut> assetLut;
		ParticleEffectHandle curvedEffect;
		{
			TestParticleEffect source(lutTemplates);
			if (source.getCurveLut() != source.getCurveLut())
				return fail("a particle effect asset baked more than one LUT");
			assetLut = source.getCurveLut();
			curvedEffect = system.createEffect(source);
		}
		if (assetLut.expired()) return fail("curve LUT died while its particle effect instance was alive");
		auto curvedEmitter = system.getEmitter(curvedEffect, 1);
		if (system.mTemplateRenderData[curvedEmitter.index].appearance[3] !=
			float(ParticleEffectCurveLut::RowsPerTemplate))
			return fail("baked LUT row offset did not reach TemplateRenderData");
		system.destroyEffect(curvedEffect);
		if (!assetLut.expired()) return fail("curve LUT outlived both its asset and particle effect instance");

		// Parent x local transform composition and per-emitter addressing.
		array<ParticleEmitterTemplate, 2> transforms{ burst(1.0f), burst(1.0f) };
		transforms[0].localTransform = glm::translate(glm::mat4(1.0f), { 2.0f, 0.0f, 0.0f });
		transforms[1].localTransform = glm::translate(glm::mat4(1.0f), { 0.0f, 3.0f, 0.0f });
		auto effect = system.createEffect(transforms, glm::translate(glm::mat4(1.0f), { 10.0f, 20.0f, 0.0f }));
		auto first = system.getEmitter(effect, 0);
		auto second = system.getEmitter(effect, 1);
		if (!first || !second || first.index == second.index) return fail("effect emitter span was not created");
		if (system.mEmitters[first.index].transform[12] != 12.0f || system.mEmitters[first.index].transform[13] != 20.0f ||
			system.mEmitters[second.index].transform[12] != 10.0f || system.mEmitters[second.index].transform[13] != 23.0f)
			return fail("effect parent and emitter-local transforms were not composed");
		system.setEffectVisible(effect, false);
		if (system.mEmitters[first.index].emissionRateAndPadding[1] != 0.0f ||
			system.mEmitters[second.index].emissionRateAndPadding[1] != 0.0f)
			return fail("effect visibility did not reach every emitter");
		system.setEffectVisibilityFlags(effect, uint32_t(ParticleEffectVisibilityFlag::Visible));
		if (system.mEmitters[first.index].emissionRateAndPadding[1] != 1.0f ||
			system.mEmitters[first.index].emissionState[1] == 0u)
			return fail("effect visibility flags changed emitter simulation state");
		system.setEmitterParameter(first, ParticleParameter::SpawnRate, 2.5f);
		system.stopEmitter(first);
		system.startEmitter(first);
		if (system.mEmitters[first.index].parameterMultipliers0[0] != 2.5f)
			return fail("start/stop changed the spawn-rate multiplier");
		if (system.mEmitters[second.index].parameterMultipliers0[0] != 1.0f)
			return fail("a per-emitter operation changed another emitter");
		system.destroyEffect(effect);

		// A reclaimed index must get a different generation, leaving stale handles inert.
		array<ParticleEmitterTemplate, 1> one{ burst(0.0f) };
		auto oldEffect = system.createEffect(one);
		auto stale = system.getEmitter(oldEffect, 0);
		step(system, 0.01f);
		if (system.isAlive(stale) || system.isAlive(oldEffect)) return fail("completed burst did not retire");
		auto replacementEffect = system.createEffect(one);
		auto replacement = system.getEmitter(replacementEffect, 0);
		if (replacement.index != stale.index || replacement.generation == stale.generation)
			return fail("reused emitter slot did not advance its generation");
		system.stopEmitter(stale);
		if (system.mEmitters[replacement.index].emissionState[1] == 0u)
			return fail("stale handle retargeted a replacement emitter");
		step(system, 0.01f);

		// Fire-and-forget bursts reclaim both emitter and effect slots without readback.
		for (size_t index = 0; index < 10000; ++index)
		{
			system.spawnEffect(one);
			step(system, 0.001f);
		}
		if (system.getLiveEmitterCount() != 0 || system.getLiveEffectCount() != 0 || system.mEmitterSlots.size() > 2)
			return fail("ten thousand fire-and-forget bursts leaked CPU slots");

		// An effect remains alive until its longest emitter has exhausted its bound.
		array<ParticleEmitterTemplate, 2> mixed{ burst(0.1f), burst(2.0f) };
		auto mixedEffect = system.createEffect(mixed);
		step(system, 0.01f); // submit both bursts
		step(system, 0.2f);
		if (system.isAlive(system.getEmitter(mixedEffect, 0))) return fail("short burst outlived its authored maximum lifetime");
		if (!system.isAlive(system.getEmitter(mixedEffect, 1)) || !system.isAlive(mixedEffect))
			return fail("effect retired before its long plume");
		auto unrelatedEffect = system.createEffect(one, glm::translate(glm::mat4(1.0f), { 7.0f, 0.0f, 0.0f }));
		auto unrelated = system.getEmitter(unrelatedEffect, 0);
		system.setEffectTransform(mixedEffect, glm::translate(glm::mat4(1.0f), { 99.0f, 0.0f, 0.0f }));
		if (system.mEmitters[unrelated.index].transform[12] != 7.0f)
			return fail("effect emitter span retargeted a reused slot");
		step(system, 2.0f);
		if (system.isAlive(mixedEffect)) return fail("effect did not retire with its long plume");

		return true;
	}
}
