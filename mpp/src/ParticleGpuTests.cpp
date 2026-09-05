#include <GL/glew.h>

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "mpp/GLErrorCheck.h"
#include "mpp/ParticleData.h"
#include "mpp/ParticleGpuTests.h"
#include "mpp/ParticleSystem.h"
#include "mpp/RawShaderProgram.h"
#include "mpp/RenderSystem.h"
#include "mpp/ResourceManager.h"
#include "mpp/ShaderStorageBuffer.h"

namespace mpp
{
	namespace
	{
		struct CounterSnapshot
		{
			ParticleCounterHeader counters{};
			uint32_t activeListIndex{ 0 };
		};

		// Test-only version of the statistics readback contract: copies are queued
		// into staging buffers and fenced. read() only consumes an already-signalled
		// result, so none of the assertions reads the live counter SSBO or waits for
		// a result from the frame that produced it.
		class LaggedCounterReadback
		{
			struct Slot
			{
				GLuint buffer{ 0 };
				GLsync fence{ nullptr };
				uint32_t activeListIndex{ 0 };
			};
			std::vector<Slot> mSlots;

		public:
			~LaggedCounterReadback()
			{
				for (auto& slot : mSlots)
				{
					if (slot.fence) glDeleteSync(slot.fence);
					if (slot.buffer) glDeleteBuffers(1, &slot.buffer);
				}
			}

			size_t enqueue(GLuint source, uint32_t activeListIndex)
			{
				Slot slot;
				slot.activeListIndex = activeListIndex;
				GL_CHECK(glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT));
				GL_CHECK(glGenBuffers(1, &slot.buffer));
				GL_CHECK(glBindBuffer(GL_COPY_WRITE_BUFFER, slot.buffer));
				GL_CHECK(glBufferData(GL_COPY_WRITE_BUFFER, sizeof(ParticleCounterHeader), nullptr, GL_STREAM_READ));
				GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, source));
				GL_CHECK(glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, sizeof(ParticleCounterHeader)));
				slot.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
				GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, 0));
				GL_CHECK(glBindBuffer(GL_COPY_WRITE_BUFFER, 0));
				mSlots.push_back(slot);
				return mSlots.size() - 1u;
			}

			bool read(size_t index, CounterSnapshot& snapshot) const
			{
				if (index >= mSlots.size()) return false;
				auto const& slot = mSlots[index];
				auto const status = glClientWaitSync(slot.fence, 0, 0);
				if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED) return false;
				GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, slot.buffer));
				GL_CHECK(glGetBufferSubData(GL_COPY_READ_BUFFER, 0, sizeof(snapshot.counters), &snapshot.counters));
				GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, 0));
				snapshot.activeListIndex = slot.activeListIndex;
				return true;
			}
		};

		ParticleEmitterTemplate burst(uint32_t count, uint32_t budget, float lifetime)
		{
			ParticleEmitterTemplate emitter;
			emitter.simulation.emissionState = { 1u, 1u, count, 0u };
			emitter.simulation.shapeSeedModulesBudget[3] = budget;
			emitter.simulation.lifetimeSizeRanges[0] = lifetime;
			emitter.simulation.lifetimeSizeRanges[1] = lifetime;
			return emitter;
		}

		uint32_t activeCount(CounterSnapshot const& snapshot)
		{
			return snapshot.activeListIndex == 0u ? snapshot.counters.activeCountA : snapshot.counters.activeCountB;
		}
	}

	bool runParticleGpuTests(RenderSystem* renderSystem, std::string* failure)
	{
		auto fail = [failure](std::string const& message)
		{
			if (failure) *failure = message;
			return false;
		};
		if (!renderSystem) return fail("RenderSystem is null");

		std::string stage = "particle system initialization";
		try
		{
			LaggedCounterReadback readback;
			struct PendingSnapshot { size_t index; std::string name; };
			std::vector<PendingSnapshot> pending;

			auto initialise = [&](ParticleSystem& system)
			{
				system.initialise();
				if (!system.mAvailable) return false;
				return true;
			};
			auto runFrame = [&](ParticleSystem& system, float dt)
			{
				system.mSimulationSeconds += dt;
				system.buildSpawnCommands(dt);
				system.ensurePoolAllocated();
				system.uploadFrameData();
				system.dispatchSpawnCommands();
				system.dispatchSimulation(dt);
				system.dispatchCompaction();
				system.retireCompletedEmitters();
			};
			auto queue = [&](ParticleSystem& system, std::string name)
			{
				pending.push_back({ readback.enqueue(system.mCounters->getBuffer(), system.mActiveListIndex), std::move(name) });
			};

			ParticleSystem& system = renderSystem->getParticleSystem();
			if (!initialise(system)) return fail("particle GPU path is unavailable");
			if (system.hasOccupiedEmitters() || system.mPoolAllocated)
				return fail("particle GPU tests require a fresh particle pool");

			stage = "spawn and expiry counters";
			std::array spawnEmitters{ burst(37u, 37u, 0.05f) };
			system.createEffect(spawnEmitters);
			runFrame(system, 0.0f);
			queue(system, "spawn");
			runFrame(system, 0.1f);
			queue(system, "expiry");

			stage = "over-spawn clamp";
			auto const capacity = renderSystem->getOptions().particlePoolCapacity;
			std::array overSpawnEmitters{ burst(capacity + 17u, capacity + 17u, 0.05f) };
			system.createEffect(overSpawnEmitters);
			runFrame(system, 0.0f);
			queue(system, "over-spawn");
			// Empty the pool before checking a small survivor list.
			runFrame(system, 0.1f);

			stage = "double-buffered active lists";
			std::array swapEmitters{ burst(23u, 23u, 10.0f) };
			system.createEffect(swapEmitters);
			runFrame(system, 0.0f);
			queue(system, "active-list frame one");
			runFrame(system, 0.01f);
			queue(system, "active-list frame two");
			// Leave the renderer-owned particle system empty for the graph GPU suite
			// that runs next under the same flag.
			runFrame(system, 10.0f);

			// Complete all queued test work before polling. The readback itself still
			// has a zero timeout and will fail rather than block on an unsignalled slot.
			GL_CHECK(glFinish());
			std::vector<CounterSnapshot> snapshots(pending.size());
			for (size_t index = 0; index < pending.size(); ++index)
				if (!readback.read(pending[index].index, snapshots[index]))
					return fail(pending[index].name + " frame-lagged counter snapshot was not ready");

			if (activeCount(snapshots[0]) != 37u) return fail("spawned active count was not N");
			if (snapshots[1].counters.freeCount != capacity || activeCount(snapshots[1]) != 0u)
				return fail("aged particles did not return every slot to the free list");
			if (activeCount(snapshots[2]) != capacity || snapshots[2].counters.droppedSpawnCount < 17u)
				return fail("over-spawn did not clamp to capacity and increment the dropped counter");
			if (snapshots[3].activeListIndex != 1u || snapshots[3].counters.activeCountB != 23u ||
				snapshots[4].activeListIndex != 0u || snapshots[4].counters.activeCountA != 23u)
				return fail("double-buffered active particle lists did not swap across two frames (frame one index/A/B " +
					std::to_string(snapshots[3].activeListIndex) + "/" + std::to_string(snapshots[3].counters.activeCountA) + "/" +
					std::to_string(snapshots[3].counters.activeCountB) + ", frame two " + std::to_string(snapshots[4].activeListIndex) + "/" +
					std::to_string(snapshots[4].counters.activeCountA) + "/" + std::to_string(snapshots[4].counters.activeCountB) + ")");

			stage = "std430 particle record stride";
			auto* spawnProgram = static_cast<RawShaderProgram*>(system.mSpawnProgram.get());
			auto const variable = glGetProgramResourceIndex(spawnProgram->getId(), GL_BUFFER_VARIABLE, "PARTICLES[0].positionAge");
			if (variable == GL_INVALID_INDEX) return fail("linked spawn kernel did not expose the particle record");
			GLenum const property = GL_TOP_LEVEL_ARRAY_STRIDE;
			GLint stride = 0;
			GL_CHECK(glGetProgramResourceiv(spawnProgram->getId(), GL_BUFFER_VARIABLE, variable, 1, &property, 1, nullptr, &stride));
			if (stride != 64 || sizeof(ParticleRecord) != 64u)
				return fail("particle record std430 array stride was " + std::to_string(stride) + " bytes instead of 64");

			GL_CHECK(glUseProgram(0));
			return true;
		}
		catch (std::exception const& error)
		{
			return fail(stage + ": " + error.what());
		}
	}
}
