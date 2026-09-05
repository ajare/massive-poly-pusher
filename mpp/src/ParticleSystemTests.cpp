#include <array>
#include <cmath>
#include <string>

#include <glm/gtc/matrix_transform.hpp>

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

		ParticleSystem system(nullptr, nullptr);

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
