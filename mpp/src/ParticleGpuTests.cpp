#include <GL/glew.h>

#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include "mpp/GLErrorCheck.h"
#include "mpp/ParticleCurveLut.h"
#include "mpp/ParticleData.h"
#include "mpp/ParticleGpuTests.h"
#include "mpp/ParticleSystem.h"
#include "mpp/ProgrammaticTextureStream.h"
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
				system.finishSimulationTiming();
				system.dispatchDepthSorts();
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
				stats.simulationGpuMilliseconds < 0.0 || stats.sortingGpuMilliseconds != 0.0 || stats.renderGpuMilliseconds != 0.0)
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

			stage = "GPU screen-space depth collision";
			RenderTextureOptions collisionDepthOptions;
			collisionDepthOptions.numAttachments = 0u;
			collisionDepthOptions.depthAttachment = RenderTextureDepthAttachment::DepthTexture;
			collisionDepthOptions.depthParams.params.minFilter = GL_NEAREST;
			collisionDepthOptions.depthParams.params.magFilter = GL_NEAREST;
			collisionDepthOptions.depthParams.params.wrap = GL_CLAMP_TO_EDGE;
			auto collisionDepthTarget = renderSystem->createRenderTexture(
				"__mpp_particle_test_collision_depth__", 16u, 16u, collisionDepthOptions);
			auto collisionDepth = std::dynamic_pointer_cast<RenderTexture>(collisionDepthTarget);
			if (!collisionDepth) return fail("screen-space collision depth target was not a render texture");
			auto projection = glm::perspective(glm::radians(60.0f), 4.0f / 3.0f, 0.1f, 100.0f);
			renderSystem->setCameraFrame(glm::mat4(1.0f), projection, { 16.0f, 16.0f }, 0.1f, 100.0f, 0.0f);
			glm::vec4 surfaceClip = projection * glm::vec4(0.0f, 0.0f, -5.0f, 1.0f);
			double surfaceDepth = double(surfaceClip.z / surfaceClip.w * 0.5f + 0.5f);
			renderSystem->pushRenderTarget(collisionDepthTarget);
			GL_CHECK(glClearDepth(surfaceDepth));
			GL_CHECK(glClear(GL_DEPTH_BUFFER_BIT));
			renderSystem->popRenderTarget();
			system.setScreenSpaceCollisionDepth(std::static_pointer_cast<Resource>(collisionDepth));
			auto screenEmitter = burst(1u, 1u, 10.0f);
			screenEmitter.localTransform = glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, -4.0f });
			screenEmitter.simulation.initialVelocityMin = { 0.0f, 0.0f, -2.0f, 0.0f };
			screenEmitter.simulation.initialVelocityMax = screenEmitter.simulation.initialVelocityMin;
			screenEmitter.simulation.lifetimeSizeRanges[2] = 0.1f;
			screenEmitter.simulation.lifetimeSizeRanges[3] = 0.1f;
			screenEmitter.simulation.shapeSeedModulesBudget[2] = uint32_t(ParticleBehaviourModule::Collision);
			screenEmitter.simulation.collisionConfiguration = { uint32_t(ParticleCollisionSource::ScreenSpace),
				uint32_t(ParticleCollisionResponse::Bounce), 0u, 0u };
			screenEmitter.simulation.collisionParameters = { 1.0f, 0.0f, 1.0f, 0.25f };
			std::array screenEmitters{ screenEmitter };
			auto screenEffect = system.createEffect(screenEmitters);
			auto screenHandle = system.getEmitter(screenEffect, 0u);
			runFrame(system, 0.0f);
			runFrame(system, 0.6f);
			GL_CHECK(glFinish());
			ParticleDrawArraysIndirectCommand screenCommand;
			GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, system.mIndirectCommands->getBuffer()));
			GL_CHECK(glGetBufferSubData(GL_COPY_READ_BUFFER,
				GLintptr(screenHandle.index * sizeof(ParticleDrawArraysIndirectCommand)), sizeof(screenCommand), &screenCommand));
			if (screenCommand.instanceCount != 1u) return fail("screen-space collision removed a bouncing particle");
			uint32_t screenParticleIndex = 0u;
			GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, system.mRenderIndices->getBuffer()));
			GL_CHECK(glGetBufferSubData(GL_COPY_READ_BUFFER, GLintptr(size_t(screenCommand.first / 4u) * sizeof(uint32_t)),
				sizeof(screenParticleIndex), &screenParticleIndex));
			ParticleRecord screenParticle;
			GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, system.mParticlePool->getBuffer()));
			GL_CHECK(glGetBufferSubData(GL_COPY_READ_BUFFER, GLintptr(size_t(screenParticleIndex) * sizeof(ParticleRecord)),
				sizeof(screenParticle), &screenParticle));
			GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, 0));
			if (screenParticle.velocityLifetime[2] < 1.9f ||
				(screenParticle.flags & uint32_t(ParticleFlag::CollisionEvent)) == 0u)
				return fail("screen-space depth collision did not reconstruct the surface and bounce");
			runFrame(system, 10.0f);
			system.setScreenSpaceCollisionDepth({});
			collisionDepth.reset();
			collisionDepthTarget.reset();
			renderSystem->getResourceManager()->deleteResourceTree("__mpp_particle_test_collision_depth__");

			stage = "GPU analytical collision responses";
			ParticleCollider floor;
			floor.shapeAndPadding[0] = uint32_t(ParticleColliderShape::Plane);
			floor.first = { 0.0f, 1.0f, 0.0f, 0.0f };
			std::array collisionWorld{ floor };
			system.setColliders(collisionWorld);
			std::array<ParticleEmitterTemplate, 5> collisionEmitters;
			for (uint32_t response = 0u; response < collisionEmitters.size(); ++response)
			{
				auto& emitter = collisionEmitters[response];
				emitter = burst(1u, 1u, 10.0f);
				emitter.localTransform = glm::translate(glm::mat4(1.0f), { 0.0f, 1.0f, -5.0f });
				emitter.simulation.initialVelocityMin = { 1.0f, -2.0f, 0.0f, 0.0f };
				emitter.simulation.initialVelocityMax = emitter.simulation.initialVelocityMin;
				emitter.simulation.lifetimeSizeRanges[2] = 0.1f;
				emitter.simulation.lifetimeSizeRanges[3] = 0.1f;
				emitter.simulation.shapeSeedModulesBudget[2] = uint32_t(ParticleBehaviourModule::Collision);
				emitter.simulation.collisionConfiguration = { uint32_t(ParticleCollisionSource::Analytical), response, 0u, 0u };
				emitter.simulation.collisionParameters = { 1.0f, 0.25f, 1.0f, 0.1f };
			}
			auto collisionEffect = system.createEffect(collisionEmitters);
			runFrame(system, 0.0f);
			runFrame(system, 0.6f);
			GL_CHECK(glFinish());
			std::array<ParticleRecord, 5> collidedParticles;
			for (uint32_t response = 0u; response < collisionEmitters.size(); ++response)
			{
				auto handle = system.getEmitter(collisionEffect, response);
				ParticleDrawArraysIndirectCommand command;
				GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, system.mIndirectCommands->getBuffer()));
				GL_CHECK(glGetBufferSubData(GL_COPY_READ_BUFFER,
					GLintptr(handle.index * sizeof(ParticleDrawArraysIndirectCommand)), sizeof(command), &command));
				if (response == uint32_t(ParticleCollisionResponse::Kill))
				{
					if (command.instanceCount != 0u) return fail("kill collision response retained its particle");
					continue;
				}
				if (command.instanceCount != 1u) return fail("non-kill collision response removed its particle");
				uint32_t particleIndex = 0u;
				GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, system.mRenderIndices->getBuffer()));
				GL_CHECK(glGetBufferSubData(GL_COPY_READ_BUFFER, GLintptr(size_t(command.first / 4u) * sizeof(uint32_t)),
					sizeof(particleIndex), &particleIndex));
				GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, system.mParticlePool->getBuffer()));
				GL_CHECK(glGetBufferSubData(GL_COPY_READ_BUFFER, GLintptr(size_t(particleIndex) * sizeof(ParticleRecord)),
					sizeof(ParticleRecord), &collidedParticles[response]));
			}
			GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, 0));
			auto collisionBits = uint32_t(ParticleFlag::Colliding) | uint32_t(ParticleFlag::CollisionEvent);
			if ((collidedParticles[0].flags & collisionBits) != collisionBits ||
				collidedParticles[0].positionAge[1] < 0.099f || collidedParticles[0].velocityLifetime[1] < 1.9f)
				return fail("bounce collision did not depenetrate and reflect velocity");
			if (std::abs(collidedParticles[1].velocityLifetime[1]) > 0.001f ||
				std::abs(collidedParticles[1].velocityLifetime[0] - 0.75f) > 0.01f)
				return fail("slide collision did not remove normal velocity and apply friction");
			if (std::abs(collidedParticles[2].velocityLifetime[0]) > 0.001f ||
				std::abs(collidedParticles[2].velocityLifetime[1]) > 0.001f)
				return fail("stop collision response retained velocity");
			if ((collidedParticles[4].flags & uint32_t(ParticleFlag::SpawnSecondaryEffect)) == 0u ||
				(collidedParticles[4].flags & uint32_t(ParticleFlag::CollisionEvent)) == 0u)
				return fail("spawn-secondary collision response did not publish a one-frame GPU event flag");
			runFrame(system, 10.0f);
			system.setColliders({});

			stage = "GPU signed distance field collision";
			auto sdfStream = std::make_shared<ProgrammaticTextureStream>(renderSystem->getResourceManager());
			sdfStream->setTarget(TextureTarget::Texture3D);
			sdfStream->setInternalFormat(TextureInternalType::Float, false, 32u, 1u);
			sdfStream->setFiltering(TextureParams::MinFilter::Linear, TextureParams::MagFilter::Linear);
			sdfStream->setWrapping(TextureParams::Wrapping::ClampToEdge);
			sdfStream->setData([](std::string const&)
			{
				constexpr size_t size = 5u;
				auto* data = new uint8_t[size * size * size * sizeof(float)];
				auto* values = reinterpret_cast<float*>(data);
				for (size_t z = 0; z < size; ++z)
					for (size_t y = 0; y < size; ++y)
						for (size_t x = 0; x < size; ++x)
							values[(z * size + y) * size + x] = float(y) - 2.0f;
				return TextureData(data, size, size, size, 32u, GL_RED, GL_FLOAT);
			});
			auto sdfTexture = renderSystem->getResourceManager()->declareResource("__mpp_particle_test_sdf__", sdfStream).first;
			sdfTexture->load();
			glm::mat4 worldToTexture(1.0f);
			worldToTexture[0][0] = 0.25f;
			worldToTexture[1][1] = 0.25f;
			worldToTexture[2][2] = 0.25f;
			worldToTexture[3] = { 0.5f, 0.5f, 1.75f, 1.0f };
			system.setSignedDistanceField(sdfTexture, worldToTexture);
			auto sdfEmitter = burst(1u, 1u, 10.0f);
			sdfEmitter.localTransform = glm::translate(glm::mat4(1.0f), { 0.0f, 1.0f, -5.0f });
			sdfEmitter.simulation.initialVelocityMin = { 0.0f, -2.0f, 0.0f, 0.0f };
			sdfEmitter.simulation.initialVelocityMax = sdfEmitter.simulation.initialVelocityMin;
			sdfEmitter.simulation.lifetimeSizeRanges[2] = 0.1f;
			sdfEmitter.simulation.lifetimeSizeRanges[3] = 0.1f;
			sdfEmitter.simulation.shapeSeedModulesBudget[2] = uint32_t(ParticleBehaviourModule::Collision);
			sdfEmitter.simulation.collisionConfiguration = { uint32_t(ParticleCollisionSource::SignedDistanceField),
				uint32_t(ParticleCollisionResponse::Bounce), 0u, 0u };
			sdfEmitter.simulation.collisionParameters[0] = 1.0f;
			std::array sdfEmitters{ sdfEmitter };
			auto sdfEffect = system.createEffect(sdfEmitters);
			auto sdfHandle = system.getEmitter(sdfEffect, 0u);
			runFrame(system, 0.0f);
			runFrame(system, 0.6f);
			GL_CHECK(glFinish());
			ParticleDrawArraysIndirectCommand sdfCommand;
			GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, system.mIndirectCommands->getBuffer()));
			GL_CHECK(glGetBufferSubData(GL_COPY_READ_BUFFER,
				GLintptr(sdfHandle.index * sizeof(ParticleDrawArraysIndirectCommand)), sizeof(sdfCommand), &sdfCommand));
			if (sdfCommand.instanceCount != 1u) return fail("SDF collision removed a bouncing particle");
			uint32_t sdfParticleIndex = 0u;
			GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, system.mRenderIndices->getBuffer()));
			GL_CHECK(glGetBufferSubData(GL_COPY_READ_BUFFER, GLintptr(size_t(sdfCommand.first / 4u) * sizeof(uint32_t)),
				sizeof(sdfParticleIndex), &sdfParticleIndex));
			ParticleRecord sdfParticle;
			GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, system.mParticlePool->getBuffer()));
			GL_CHECK(glGetBufferSubData(GL_COPY_READ_BUFFER, GLintptr(size_t(sdfParticleIndex) * sizeof(ParticleRecord)),
				sizeof(sdfParticle), &sdfParticle));
			GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, 0));
			if (sdfParticle.positionAge[1] < 0.099f || sdfParticle.velocityLifetime[1] < 1.9f ||
				(sdfParticle.flags & uint32_t(ParticleFlag::CollisionEvent)) == 0u)
				return fail("SDF collision did not sample, depenetrate, and bounce (y=" +
					std::to_string(sdfParticle.positionAge[1]) + ", vy=" + std::to_string(sdfParticle.velocityLifetime[1]) +
					", flags=" + std::to_string(sdfParticle.flags) + ")");
			runFrame(system, 10.0f);
			system.clearSignedDistanceField();
			sdfTexture.reset();
			renderSystem->getResourceManager()->deleteResourceTree("__mpp_particle_test_sdf__");

			stage = "GPU alpha depth radix sort";
			if (system.mSortRecordsA || system.mSortRecordsB || system.mRadixHistogram || system.mSortDispatchCommand ||
				renderSystem->getResourceManager()->getResource("__mpp_particle_sort_keys__", true))
				return fail("additive and weighted OIT appearances allocated depth-sort resources");
			auto sorted = burst(257u, 257u, 10.0f);
			sorted.simulation.shapeSeedModulesBudget[0] = uint32_t(ParticleSpawnShape::Line);
			sorted.simulation.shapeParameters = { 0.0f, 0.0f, 8.0f, 0.0f };
			sorted.localTransform = glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, -20.0f });
			sorted.appearance.modes[3] = uint32_t(ParticleBlendClass::Alpha);
			sorted.appearance.sorting[0] = uint32_t(ParticleSortMode::BackToFront);
			std::array sortedEmitters{ sorted };
			auto sortedEffect = system.createEffect(sortedEmitters);
			auto sortedEmitter = system.getEmitter(sortedEffect, 0u);
			runFrame(system, 0.0f);
			GL_CHECK(glFinish());
			if (!system.mSortRecordsA || !system.mSortRecordsB || !system.mRadixHistogram || !system.mSortDispatchCommand ||
				!renderSystem->getResourceManager()->getResource("__mpp_particle_sort_keys__", true))
				return fail("an opted-in alpha appearance did not allocate depth-sort resources");
			ParticleDrawArraysIndirectCommand sortedCommand;
			GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, system.mIndirectCommands->getBuffer()));
			GL_CHECK(glGetBufferSubData(GL_COPY_READ_BUFFER,
				GLintptr(sortedEmitter.index * sizeof(ParticleDrawArraysIndirectCommand)), sizeof(sortedCommand), &sortedCommand));
			if (sortedCommand.instanceCount != 257u) return fail("depth-sort test appearance did not retain every visible particle");
			std::vector<uint32_t> sortedIndices(sortedCommand.instanceCount);
			uint32_t const sortedOffset = sortedCommand.first / 4u;
			GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, system.mRenderIndices->getBuffer()));
			GL_CHECK(glGetBufferSubData(GL_COPY_READ_BUFFER, GLintptr(size_t(sortedOffset) * sizeof(uint32_t)),
				GLsizeiptr(sortedIndices.size() * sizeof(uint32_t)), sortedIndices.data()));
			float previousDepth = std::numeric_limits<float>::infinity();
			for (uint32_t particleIndex : sortedIndices)
			{
				ParticleRecord particle;
				GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, system.mParticlePool->getBuffer()));
				GL_CHECK(glGetBufferSubData(GL_COPY_READ_BUFFER, GLintptr(size_t(particleIndex) * sizeof(ParticleRecord)),
					sizeof(particle), &particle));
				float const depth = -particle.positionAge[2];
				if (depth > previousDepth) return fail("alpha render indices were not radix-sorted back-to-front");
				previousDepth = depth;
			}
			GL_CHECK(glBindBuffer(GL_COPY_READ_BUFFER, 0));
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
