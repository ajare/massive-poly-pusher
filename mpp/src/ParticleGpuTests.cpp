#include <GL/glew.h>

#include <array>
#include <cstdint>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include "mpp/GLErrorCheck.h"
#include "mpp/ParticleCurveLut.h"
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

		ParticleEmitterTemplate cullingBurst(glm::vec3 position, float size = 1.0f)
		{
			auto emitter = burst(1u, 1u, 10.0f);
			emitter.localTransform = glm::translate(glm::mat4(1.0f), position);
			emitter.simulation.lifetimeSizeRanges[2] = size;
			emitter.simulation.lifetimeSizeRanges[3] = size;
			return emitter;
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
			stage = "RGBA16F particle effect curve LUT";
			ParticleEmitterTemplate hdrTemplate;
			hdrTemplate.curves[size_t(ParticleScalarCurve::EmissiveIntensity)].keys = {
				{ 0.0f, 1.0f }, { 1.0f, 8.5f }
			};
			std::array hdrTemplates{ hdrTemplate };
			auto hdrLut = ParticleEffectCurveLut::bake(hdrTemplates);
			hdrLut->bind(1u);
			GLint internalFormat = 0;
			GL_CHECK(glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat));
			if (internalFormat != GL_RGBA16F) return fail("particle effect LUT was not allocated as RGBA16F");
			std::vector<float> lutReadback(size_t(hdrLut->getWidth()) * hdrLut->getHeight() * 4u);
			GL_CHECK(glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, lutReadback.data()));
			size_t const emissiveEndpoint =
				(size_t(ParticleEffectCurveLut::SampleCount) + ParticleEffectCurveLut::SampleCount - 1u) * 4u + 1u;
			if (std::abs(lutReadback[emissiveEndpoint] - 8.5f) > 0.01f)
				return fail("emissive intensity above one did not survive the RGBA16F bake and upload");
			GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
			hdrLut->unload();

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
				system.advanceStatisticsFrame();
				system.mSimulationSeconds += dt;
				system.buildSpawnCommands(dt);
				system.ensurePoolAllocated();
				system.uploadFrameData();
				system.dispatchStatisticsPrepare();
				system.beginStatisticsSample();
				system.dispatchSpawnCommands();
				system.dispatchSimulation(dt);
				system.dispatchCompaction();
				system.retireCompletedEmitters();
				system.finishStatisticsSample();
			};
			auto queue = [&](ParticleSystem& system, std::string name)
			{
				pending.push_back({ readback.enqueue(system.mCounters->getBuffer(), system.mActiveListIndex), std::move(name) });
			};

			ParticleSystem& system = renderSystem->getParticleSystem();
			if (!initialise(system)) return fail("particle GPU path is unavailable");
			if (system.isStatisticsEnabled() || system.getStats().valid)
				return fail("particle statistics were not off by default");
			if (system.hasOccupiedEmitters() || system.mPoolAllocated)
				return fail("particle GPU tests require a fresh particle pool");

			system.setStatisticsEnabled(true);

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
			system.advanceStatisticsFrame();
			auto const stats = system.getStats();
			if (!stats.valid || stats.framesLagged < 2u || stats.activeParticles != 23u ||
				stats.freeParticles != capacity - 23u || stats.spawnedParticles != 0u ||
				stats.killedParticles != 0u || stats.droppedParticles != 0u ||
				stats.renderedParticles != 23u || stats.culledParticles != 0u ||
				stats.activeEmitters != 1u || stats.capacity != capacity || stats.capacityUsage <= 0.0f ||
				stats.simulationGpuMilliseconds < 0.0 || stats.renderGpuMilliseconds != 0.0)
				return fail("public ParticleStats did not publish the complete frame-lagged GPU snapshot");
			system.setStatisticsEnabled(false);

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

			stage = "GPU visibility compaction";
			renderSystem->setCameraFrame(glm::mat4(1.0f),
				glm::perspective(glm::radians(60.0f), 4.0f / 3.0f, 0.1f, 100.0f),
				{ 800.0f, 600.0f }, 0.1f, 100.0f, 0.0f);
			auto visible = cullingBurst({ 0.0f, 0.0f, -5.0f });
			auto outsideFrustum = cullingBurst({ 100.0f, 0.0f, -5.0f });
			auto beyondDistance = cullingBurst({ 0.0f, 0.0f, -50.0f });
			beyondDistance.appearance.culling[0] = 10.0f;
			auto belowProjectedSize = cullingBurst({ 0.0f, 0.0f, -20.0f }, 0.001f);
			belowProjectedSize.appearance.culling[1] = 2.0f;
			auto hidden = cullingBurst({ 0.0f, 0.0f, -5.0f });
			std::array<ParticleEmitterTemplate, 1> oneVisible{ visible }, oneFrustum{ outsideFrustum },
				oneDistance{ beyondDistance }, oneProjected{ belowProjectedSize }, oneHidden{ hidden };
			system.createEffect(oneVisible);
			system.createEffect(oneFrustum);
			system.createEffect(oneDistance);
			system.createEffect(oneProjected);
			auto hiddenEffect = system.createEffect(oneHidden);
			system.setEffectVisible(hiddenEffect, false);
			runFrame(system, 0.0f);
			auto cullingSnapshotIndex = readback.enqueue(system.mCounters->getBuffer(), system.mActiveListIndex);
			GL_CHECK(glFinish());
			CounterSnapshot cullingSnapshot;
			if (!readback.read(cullingSnapshotIndex, cullingSnapshot)) return fail("GPU culling snapshot was not ready");
			if (activeCount(cullingSnapshot) != 5u || cullingSnapshot.counters.renderedCount != 1u ||
				cullingSnapshot.counters.culledCount != 4u)
				return fail("frustum, distance, projected-size, or effect visibility culling did not compact five survivors to one draw");
			runFrame(system, 10.0f);

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
