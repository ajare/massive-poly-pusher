#include <algorithm>
#include <chrono>
#include <exception>
#include <string>
#include <vector>

#include <GL/glew.h>
#include <GL/gl.h>

#include "mpp/ComputeProgram.h"
#include "mpp/GLErrorCheck.h"
#include "mpp/GpuDebugScope.h"
#include "mpp/MppException.h"
#include "mpp/ParticleDrawProgram.h"
#include "mpp/ParticleShaders.h"
#include "mpp/ParticleSystem.h"
#include "mpp/RawShaderStream.h"
#include "mpp/RenderSystem.h"
#include "mpp/ResourceManager.h"
#include "mpp/ShaderStorageBuffer.h"
#include "PersistentMappedBuffer.h"

using namespace std;

namespace mpp
{
	namespace
	{
		char const* PoolInitialiseProgramName = "__mpp_particle_pool_initialise__";
		char const* SpawnProgramName = "__mpp_particle_spawn__";
		char const* SimulationPrepareProgramName = "__mpp_particle_simulation_prepare__";
		char const* SimulationProgramName = "__mpp_particle_simulation__";
		char const* CompactionPrepareProgramName = "__mpp_particle_compaction_prepare__";
		char const* CompactionCountProgramName = "__mpp_particle_compaction_count__";
		char const* CompactionPrefixProgramName = "__mpp_particle_compaction_prefix__";
		char const* CompactionScatterProgramName = "__mpp_particle_compaction_scatter__";
		char const* DrawProgramName = "__mpp_particle_draw_additive__";

		constexpr uint32_t ParticlePoolBinding = 0;
		constexpr uint32_t FreeIndicesBinding = 1;
		constexpr uint32_t ActiveIndicesABinding = 2;
		constexpr uint32_t ActiveIndicesBBinding = 3;
		constexpr uint32_t CountersBinding = 4;
		constexpr uint32_t EmitterBinding = 5;
		// Bindings six and seven are stage-local aliases. Template data is draw-only;
		// scratch, spawn, dispatch, and indirect buffers are never read together.
		constexpr uint32_t TemplateRenderBinding = 6;
		constexpr uint32_t CompactionScratchBinding = 6;
		constexpr uint32_t SpawnCommandBinding = 7;
		constexpr uint32_t DispatchCommandBinding = 7;
		constexpr uint32_t IndirectCommandBinding = 7;
		constexpr uint32_t RequiredStorageBindings = 8;
		constexpr size_t DispatchCommandBytes = 3 * sizeof(uint32_t);
		constexpr uint32_t NoiseTextureSize = 16;

		template<typename T>
		size_t bytes(vector<T> const& values)
		{
			return values.size() * sizeof(T);
		}

		void setTranslation(EmitterSimData& emitter, float x, float y, float z)
		{
			emitter.transform[12] = x;
			emitter.transform[13] = y;
			emitter.transform[14] = z;
		}
	}

	ParticleSystem::ParticleSystem(RenderSystem* renderSystem, ResourceManager* resourceManager)
		: mwRenderSystem(renderSystem)
		, mwResourceManager(resourceManager)
	{
	}

	ParticleSystem::~ParticleSystem()
	{
		if (mNoiseTexture != 0)
		{
			glDeleteTextures(1, &mNoiseTexture);
			mNoiseTexture = 0;
		}
		if (mVertexArray != 0)
		{
			glDeleteVertexArrays(1, &mVertexArray);
			mVertexArray = 0;
		}

		mSpawnCommandBuffer.reset();
		mTemplateRenderBuffer.reset();
		mEmitterBuffer.reset();
		mCompactionDispatchCommand.reset();
		mSimulationDispatchCommand.reset();
		mIndirectCommands.reset();
		mCompactionScratch.reset();
		mCounters.reset();
		mRenderIndices.reset();
		mActiveIndicesB.reset();
		mActiveIndicesA.reset();
		mFreeIndices.reset();
		mParticlePool.reset();

		auto releaseProgram = [this](ResourcePtr& program)
		{
			if (!program) return;
			program->release(mwRenderSystem);
			if (!program->isReferenced()) program->destroy();
			program.reset();
		};
		releaseProgram(mPoolInitialiseProgram);
		releaseProgram(mSpawnProgram);
		releaseProgram(mSimulationPrepareProgram);
		releaseProgram(mSimulationProgram);
		releaseProgram(mCompactionPrepareProgram);
		releaseProgram(mCompactionCountProgram);
		releaseProgram(mCompactionPrefixProgram);
		releaseProgram(mCompactionScatterProgram);
		releaseProgram(mDrawProgram);
	}

	void ParticleSystem::disableWithWarning(string const& reason)
	{
		if (!mAvailable) return;
		mwRenderSystem->warnMessage("The particle system is disabled and no particles will be drawn. " + reason);
		mAvailable = false;
	}

	void ParticleSystem::initialise()
	{
		if (mInitialised) return;
		mInitialised = true;

		auto const& caps = mwRenderSystem->getCaps();
		if (!caps.supportsCompute)
		{
			mwRenderSystem->warnMessage("This OpenGL context reports no compute shader support. The particle system is disabled and no particles will be drawn.");
			return;
		}
		if (!caps.supportsMultiDrawIndirect)
		{
			mwRenderSystem->warnMessage("This OpenGL context reports no multi-draw indirect support. The particle system is disabled and no particles will be drawn.");
			return;
		}

		try
		{
			mWorkGroupSize = max(1u, min<uint32_t>(64, min(caps.maxComputeWorkGroupSize[0], caps.maxComputeWorkGroupInvocations)));
			if (caps.maxShaderStorageBufferBindings < RequiredStorageBindings)
			{
				THROW_MPP("The particle system needs " + to_string(RequiredStorageBindings) +
					" shader storage buffer bindings; this context reports " +
					to_string(caps.maxShaderStorageBufferBindings) + ".", __LINE__, __FILE__, __func__);
			}

			auto createComputeProgram = [this](char const* name, char const* source)
			{
				auto stream = make_shared<ComputeProgramStream>(mwResourceManager);
				stream->setSource(RawShaderStage::Compute, source);
				stream->setDefine("MPP_PARTICLE_WORK_GROUP_SIZE", to_string(mWorkGroupSize));
				auto program = mwResourceManager->declareResource(name, stream).first;
				program->acquire(mwRenderSystem);
				program->load();
				return program;
			};
			mPoolInitialiseProgram = createComputeProgram(PoolInitialiseProgramName, ParticlePoolInitialiseComputeShader);
			mSpawnProgram = createComputeProgram(SpawnProgramName, ParticleSpawnComputeShader);
			mSimulationPrepareProgram = createComputeProgram(SimulationPrepareProgramName, ParticleSimulationPrepareComputeShader);
			mSimulationProgram = createComputeProgram(SimulationProgramName, ParticleSimulationComputeShader);
			mCompactionPrepareProgram = createComputeProgram(CompactionPrepareProgramName, ParticleCompactionPrepareComputeShader);
			mCompactionCountProgram = createComputeProgram(CompactionCountProgramName, ParticleCompactionCountComputeShader);
			mCompactionPrefixProgram = createComputeProgram(CompactionPrefixProgramName, ParticleCompactionPrefixComputeShader);
			mCompactionScatterProgram = createComputeProgram(CompactionScatterProgramName, ParticleCompactionScatterComputeShader);
			createNoiseTexture();

			auto drawStream = make_shared<ParticleDrawProgramStream>(mwResourceManager);
			drawStream->setSource(RawShaderStage::Vertex, ParticleDrawVertexShader);
			drawStream->setSource(RawShaderStage::Fragment, ParticleDrawFragmentShader);
			drawStream->setDefine("MPP_PARTICLE_BLEND_ADDITIVE", "1");
			mDrawProgram = mwResourceManager->declareResource(DrawProgramName, drawStream).first;
			mDrawProgram->acquire(mwRenderSystem);
			mDrawProgram->load();

			GL_CHECK(glGenVertexArrays(1, &mVertexArray));
			if (mVertexArray == 0)
				THROW_MPP("Could not create the particle vertex array object.", __LINE__, __FILE__, __func__);

			mAvailable = true;
		}
		catch (exception const& error)
		{
			// A driver may report compute support and still refuse a valid kernel.
			mwRenderSystem->warnMessage(string("The particle system could not be initialised and is disabled; no particles will be drawn. ") + error.what());
			mAvailable = false;
		}
	}

	void ParticleSystem::createNoiseTexture()
	{
		if (mNoiseTexture != 0) return;

		vector<uint8_t> texels(size_t(NoiseTextureSize) * NoiseTextureSize * NoiseTextureSize * 4u);
		auto hashValue = [](uint32_t value)
		{
			value ^= value >> 16u;
			value *= 0x7feb352du;
			value ^= value >> 15u;
			value *= 0x846ca68bu;
			value ^= value >> 16u;
			return value;
		};
		for (size_t texel = 0; texel < texels.size() / 4u; ++texel)
		{
			uint32_t state = hashValue(uint32_t(texel) ^ 0x6d2b79f5u);
			for (size_t channel = 0; channel < 3; ++channel)
			{
				state = hashValue(state + uint32_t(channel));
				texels[texel * 4u + channel] = uint8_t(state >> 24u);
			}
			texels[texel * 4u + 3u] = 255u;
		}

		GL_CHECK(glGenTextures(1, &mNoiseTexture));
		if (mNoiseTexture == 0)
			THROW_MPP("Could not create the particle noise texture.", __LINE__, __FILE__, __func__);
		GL_CHECK(glBindTexture(GL_TEXTURE_3D, mNoiseTexture));
		GL_CHECK(glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		GL_CHECK(glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GL_CHECK(glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT));
		GL_CHECK(glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT));
		GL_CHECK(glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT));
		GL_CHECK(glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, NoiseTextureSize, NoiseTextureSize, NoiseTextureSize,
			0, GL_RGBA, GL_UNSIGNED_BYTE, texels.data()));
		GL_CHECK(glObjectLabel(GL_TEXTURE, mNoiseTexture, -1, "Particle 3D noise"));
		GL_CHECK(glBindTexture(GL_TEXTURE_3D, 0));
	}

	void ParticleSystem::ensurePoolAllocated()
	{
		if (mPoolAllocated) return;
		if (mEmitters.empty())
			THROW_MPP("A particle pool cannot be allocated before the first emitter is created.", __LINE__, __FILE__, __func__);

		auto const configuredCapacity = mwRenderSystem->getOptions().particlePoolCapacity;
		if (configuredCapacity < MinimumParticlePoolCapacity || configuredCapacity > MaximumParticlePoolCapacity)
		{
			THROW_MPP("Particle pool capacity " + to_string(configuredCapacity) + " is outside the supported range " +
				to_string(MinimumParticlePoolCapacity) + " to " + to_string(MaximumParticlePoolCapacity) + ".",
				__LINE__, __FILE__, __func__);
		}
		mPoolCapacity = configuredCapacity;

		size_t const poolBytes = size_t(mPoolCapacity) * sizeof(ParticleRecord);
		size_t const indexBytes = size_t(mPoolCapacity) * sizeof(uint32_t);
		size_t const counterBytes = sizeof(ParticleCounterHeader) + size_t(MaxTemplateCount) * sizeof(uint32_t);
		size_t const emitterBytes = size_t(MaxEmitterCount) * sizeof(EmitterSimData);
		size_t const templateBytes = size_t(MaxTemplateCount) * sizeof(TemplateRenderData);
		size_t const commandBytes = size_t(MaxSpawnCommandCount) * sizeof(ParticleSpawnCommand);
		size_t const compactionScratchBytes = size_t(MaxTemplateCount) * 2u * sizeof(uint32_t);
		size_t const indirectCommandBytes = size_t(MaxTemplateCount) * sizeof(ParticleDrawArraysIndirectCommand);
		size_t const largestBlock = max({ poolBytes, indexBytes, counterBytes, emitterBytes, templateBytes, commandBytes,
			compactionScratchBytes, indirectCommandBytes, DispatchCommandBytes });
		if (largestBlock > mwRenderSystem->getCaps().maxShaderStorageBlockSize)
		{
			THROW_MPP("The configured particle buffers need a shader storage block of " + to_string(largestBlock) +
				" bytes, exceeding the GPU maximum of " + to_string(mwRenderSystem->getCaps().maxShaderStorageBlockSize) + " bytes.",
				__LINE__, __FILE__, __func__);
		}

		mParticlePool = make_unique<ShaderStorageBuffer>();
		mParticlePool->create(poolBytes, nullptr, "Particle pool");
		mFreeIndices = make_unique<ShaderStorageBuffer>();
		mFreeIndices->create(indexBytes, nullptr, "Particle free indices");
		mActiveIndicesA = make_unique<ShaderStorageBuffer>();
		mActiveIndicesA->create(indexBytes, nullptr, "Particle active indices A");
		mActiveIndicesB = make_unique<ShaderStorageBuffer>();
		mActiveIndicesB->create(indexBytes, nullptr, "Particle active indices B");
		mRenderIndices = make_unique<ShaderStorageBuffer>();
		mRenderIndices->create(indexBytes, nullptr, "Particle render indices by template");
		mCounters = make_unique<ShaderStorageBuffer>();
		mCounters->create(counterBytes, nullptr, "Particle counters and template live counts");
		mCompactionScratch = make_unique<ShaderStorageBuffer>();
		mCompactionScratch->create(compactionScratchBytes, nullptr, "Particle template offsets and scatter cursors");
		mIndirectCommands = make_unique<ShaderStorageBuffer>();
		mIndirectCommands->create(indirectCommandBytes, nullptr, "Particle indirect draw commands by template");
		mSimulationDispatchCommand = make_unique<ShaderStorageBuffer>();
		mSimulationDispatchCommand->create(DispatchCommandBytes, nullptr, "Particle simulation dispatch command");
		mCompactionDispatchCommand = make_unique<ShaderStorageBuffer>();
		mCompactionDispatchCommand->create(DispatchCommandBytes, nullptr, "Particle compaction dispatch command");

		GLint storageAlignment = 1;
		GL_CHECK(glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &storageAlignment));
		bool const persistent = mwRenderSystem->getCaps().streamingGeometry;
		mEmitterBuffer = make_unique<detail::PersistentMappedBuffer>();
		mEmitterBuffer->create(GL_SHADER_STORAGE_BUFFER, emitterBytes, max(1, storageAlignment), persistent,
			mEmitters.data(), bytes(mEmitters), "Particle emitter simulation data");
		mTemplateRenderBuffer = make_unique<detail::PersistentMappedBuffer>();
		mTemplateRenderBuffer->create(GL_SHADER_STORAGE_BUFFER, templateBytes, max(1, storageAlignment), persistent,
			mTemplateRenderData.data(), bytes(mTemplateRenderData), "Particle emitter template render data");
		mSpawnCommandBuffer = make_unique<detail::PersistentMappedBuffer>();
		mSpawnCommandBuffer->create(GL_SHADER_STORAGE_BUFFER, commandBytes, max(1, storageAlignment), persistent,
			mSpawnCommands.data(), bytes(mSpawnCommands), "Particle spawn commands");

		mFreeIndices->bindStorage(FreeIndicesBinding);
		mCounters->bindStorage(CountersBinding);
		auto* initialiseProgram = static_cast<ComputeProgram*>(mPoolInitialiseProgram.get());
		initialiseProgram->use();
		initialiseProgram->setUniform("POOL_CAPACITY", mPoolCapacity);
		initialiseProgram->setUniform("TEMPLATE_CAPACITY", MaxTemplateCount);
		uint32_t const initialisedValues = max(mPoolCapacity, MaxTemplateCount);
		initialiseProgram->dispatch((initialisedValues + mWorkGroupSize - 1) / mWorkGroupSize);
		GL_CHECK(glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT));
		mPoolAllocated = true;
	}

	void ParticleSystem::createBootstrapEmitters()
	{
		// Keeps the vertical slice observable until authored particle effects and
		// the public emitter API arrive. The emitters exercise every spawn shape and
		// every independent combination needed to keep module branches live.
		constexpr uint32_t initialParticlesPerShape = 24;
		constexpr uint32_t particlesPerShapeBudget = 128;
		constexpr uint32_t moduleMasks[]{
			0u,
			uint32_t(ParticleBehaviourModule::Gravity),
			uint32_t(ParticleBehaviourModule::Drag),
			uint32_t(ParticleBehaviourModule::Noise),
			uint32_t(ParticleBehaviourModule::Gravity) | uint32_t(ParticleBehaviourModule::Drag),
			uint32_t(ParticleBehaviourModule::Gravity) | uint32_t(ParticleBehaviourModule::Noise),
			uint32_t(ParticleBehaviourModule::Gravity) | uint32_t(ParticleBehaviourModule::Drag) | uint32_t(ParticleBehaviourModule::Noise)
		};
		for (uint32_t shape = uint32_t(ParticleSpawnShape::Point); shape <= uint32_t(ParticleSpawnShape::Cone); ++shape)
		{
			EmitterSimData emitter;
			setTranslation(emitter, (float(shape) - 3.0f) * 0.55f, -0.15f, 0.0f);
			emitter.initialVelocityMin = { -0.1f, 0.15f, -0.1f, 0.0f };
			emitter.initialVelocityMax = { 0.1f, 0.45f, 0.1f, 0.0f };
			emitter.lifetimeSizeRanges = { 4.0f, 6.0f, 0.035f, 0.065f };
			emitter.rotationRanges = { -3.14159f, 3.14159f, -1.0f, 1.0f };
			emitter.shapeSeedModulesBudget = { shape, 0x6d2b79f5u + shape * 977u, moduleMasks[shape], particlesPerShapeBudget };
			emitter.emissionRateAndPadding[0] = shape == 0u ? 0.5f : 8.0f + float(shape);
			emitter.gravityAndDrag = { 0.0f, -0.35f, 0.0f, 0.45f };
			emitter.noiseFrequencyStrength = { 0.65f, 0.65f, 0.65f, 0.3f };
			emitter.noiseScrollAndTimeScale = { 0.07f, 0.11f, 0.05f, 1.0f };
			emitter.colourMin = { 0.2f + float(shape % 3u) * 0.25f, 0.35f, 0.6f, 0.75f };
			emitter.colourMax = { 1.0f, 0.65f + float(shape % 2u) * 0.25f, 1.0f, 1.0f };
			switch (ParticleSpawnShape(shape))
			{
			case ParticleSpawnShape::Point: break;
			case ParticleSpawnShape::Line: emitter.shapeParameters = { 0.22f, 0.16f, 0.0f, 0.0f }; break;
			case ParticleSpawnShape::Box: emitter.shapeParameters = { 0.18f, 0.18f, 0.18f, 0.0f }; break;
			case ParticleSpawnShape::Sphere: emitter.shapeParameters = { 0.24f, 0.0f, 0.0f, 0.0f }; break;
			case ParticleSpawnShape::Hemisphere: emitter.shapeParameters = { 0.24f, 0.0f, 0.0f, 0.0f }; break;
			case ParticleSpawnShape::Disc: emitter.shapeParameters = { 0.27f, 0.0f, 0.0f, 0.0f }; break;
			case ParticleSpawnShape::Cone: emitter.shapeParameters = { 0.22f, 0.42f, 0.0f, 0.0f }; break;
			}

			uint32_t const emitterIndex = uint32_t(mEmitters.size());
			emitter.emissionState[3] = uint32_t(mTemplateRenderData.size());
			mEmitters.push_back(emitter);
			mTemplateRenderData.emplace_back();
			mEmitterFrameStates.push_back({ {}, initialParticlesPerShape, false });
			mSpawnCommands.push_back({ emitterIndex, initialParticlesPerShape, 0x9e3779b9u + shape, 0u });
		}
	}

	void ParticleSystem::buildSpawnCommands(float dt)
	{
		if (mEmitterFrameStates.size() != mEmitters.size())
			THROW_MPP("Particle emitter frame state does not match the emitter table.", __LINE__, __FILE__, __func__);

		for (uint32_t emitterIndex = 0; emitterIndex < uint32_t(mEmitters.size()); ++emitterIndex)
		{
			auto const& emitter = mEmitters[emitterIndex];
			auto& frame = mEmitterFrameStates[emitterIndex];
			if (emitter.emissionState[1] == 0u) continue;

			uint32_t spawnCount = 0;
			if (emitter.emissionState[0] == 0u)
			{
				spawnCount = frame.spawnAccumulator.accumulate(
					emitter.emissionRateAndPadding[0], emitter.parameterMultipliers0[0], dt);
			}
			else if (!frame.burstSubmitted)
			{
				spawnCount = emitter.emissionState[2];
				frame.burstSubmitted = true;
			}

			if (spawnCount == 0u) continue;
			mSpawnCommands.push_back({ emitterIndex, spawnCount, 0x9e3779b9u + emitterIndex, frame.spawnCounter });
			frame.spawnCounter += spawnCount;
		}
	}

	void ParticleSystem::uploadFrameData()
	{
		if (mEmitters.size() > MaxEmitterCount || mTemplateRenderData.size() > MaxTemplateCount ||
			mSpawnCommands.size() > MaxSpawnCommandCount)
			THROW_MPP("Particle emitter-template, emitter, or spawn-command capacity was exceeded.", __LINE__, __FILE__, __func__);
		if (mTemplateRenderData.empty())
			THROW_MPP("Particle emitters require at least one emitter template.", __LINE__, __FILE__, __func__);
		if (!is_sorted(mTemplateRenderData.begin(), mTemplateRenderData.end(), [](auto const& left, auto const& right)
			{ return left.modes[3] < right.modes[3]; }))
			THROW_MPP("Particle emitter templates must be ordered by blend class before upload.", __LINE__, __FILE__, __func__);
		for (auto const& emitter : mEmitters)
			if (emitter.emissionState[3] >= mTemplateRenderData.size())
				THROW_MPP("A particle emitter references an invalid emitter-template index.", __LINE__, __FILE__, __func__);

		mEmitterBuffer->upload(mEmitters.data(), bytes(mEmitters), 0, bytes(mEmitters));
		mTemplateRenderBuffer->upload(mTemplateRenderData.data(), bytes(mTemplateRenderData), 0, bytes(mTemplateRenderData));
		if (!mSpawnCommands.empty())
			mSpawnCommandBuffer->upload(mSpawnCommands.data(), bytes(mSpawnCommands), 0, bytes(mSpawnCommands));
	}

	void ParticleSystem::dispatchSpawnCommands()
	{
		if (mSpawnCommands.empty()) return;

		mParticlePool->bindStorage(ParticlePoolBinding);
		mFreeIndices->bindStorage(FreeIndicesBinding);
		mActiveIndicesA->bindStorage(ActiveIndicesABinding);
		mActiveIndicesB->bindStorage(ActiveIndicesBBinding);
		mCounters->bindStorage(CountersBinding);
		GL_CHECK(glBindBufferRange(GL_SHADER_STORAGE_BUFFER, EmitterBinding, mEmitterBuffer->getBuffer(),
			static_cast<GLintptr>(mEmitterBuffer->getActiveOffset()), static_cast<GLsizeiptr>(bytes(mEmitters))));
		GL_CHECK(glBindBufferRange(GL_SHADER_STORAGE_BUFFER, SpawnCommandBinding, mSpawnCommandBuffer->getBuffer(),
			static_cast<GLintptr>(mSpawnCommandBuffer->getActiveOffset()), static_cast<GLsizeiptr>(bytes(mSpawnCommands))));

		auto* spawnProgram = static_cast<ComputeProgram*>(mSpawnProgram.get());
		spawnProgram->use();
		spawnProgram->setUniform("EMITTER_COUNT", uint32_t(mEmitters.size()));
		spawnProgram->setUniform("TEMPLATE_COUNT", uint32_t(mTemplateRenderData.size()));
		spawnProgram->setUniform("SPAWN_COMMAND_OFFSET", 0u);
		spawnProgram->setUniform("ACTIVE_LIST_INDEX", mActiveListIndex);
		spawnProgram->dispatch(uint32_t(mSpawnCommands.size()));
		GL_CHECK(glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT));
		mSpawnCommandBuffer->markUsed();
		mSpawnCommands.clear();
	}

	void ParticleSystem::dispatchSimulation(float dt)
	{
		mCounters->bindStorage(CountersBinding);
		mSimulationDispatchCommand->bindStorage(DispatchCommandBinding);
		auto* prepareProgram = static_cast<ComputeProgram*>(mSimulationPrepareProgram.get());
		prepareProgram->use();
		prepareProgram->setUniform("ACTIVE_LIST_INDEX", mActiveListIndex);
		prepareProgram->dispatch(1u);
		GL_CHECK(glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT));

		mParticlePool->bindStorage(ParticlePoolBinding);
		mFreeIndices->bindStorage(FreeIndicesBinding);
		mActiveIndicesA->bindStorage(ActiveIndicesABinding);
		mActiveIndicesB->bindStorage(ActiveIndicesBBinding);
		mCounters->bindStorage(CountersBinding);
		GL_CHECK(glBindBufferRange(GL_SHADER_STORAGE_BUFFER, EmitterBinding, mEmitterBuffer->getBuffer(),
			static_cast<GLintptr>(mEmitterBuffer->getActiveOffset()), static_cast<GLsizeiptr>(bytes(mEmitters))));

		auto* simulationProgram = static_cast<ComputeProgram*>(mSimulationProgram.get());
		simulationProgram->use();
		simulationProgram->setUniform("ACTIVE_LIST_INDEX", mActiveListIndex);
		simulationProgram->setUniform("EMITTER_COUNT", uint32_t(mEmitters.size()));
		simulationProgram->setUniform("TEMPLATE_COUNT", uint32_t(mTemplateRenderData.size()));
		simulationProgram->setUniform("DELTA_SECONDS", dt);
		simulationProgram->setUniform("SIMULATION_SECONDS", mSimulationSeconds);
		simulationProgram->setUniform("NOISE_TEXTURE", int32_t(0));
		GL_CHECK(glActiveTexture(GL_TEXTURE0));
		GL_CHECK(glBindSampler(0, 0));
		GL_CHECK(glBindTexture(GL_TEXTURE_3D, mNoiseTexture));
		mSimulationDispatchCommand->bindDispatchIndirect();
		simulationProgram->dispatchIndirect();
		GL_CHECK(glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0));
		GL_CHECK(glBindTexture(GL_TEXTURE_3D, 0));
		GL_CHECK(glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT));

		mActiveListIndex = 1u - mActiveListIndex;
	}

	void ParticleSystem::dispatchCompaction()
	{
		uint32_t const templateCount = uint32_t(mTemplateRenderData.size());

		mCounters->bindStorage(CountersBinding);
		mCompactionScratch->bindStorage(CompactionScratchBinding);
		mCompactionDispatchCommand->bindStorage(DispatchCommandBinding);
		auto* prepareProgram = static_cast<ComputeProgram*>(mCompactionPrepareProgram.get());
		prepareProgram->use();
		prepareProgram->setUniform("ACTIVE_LIST_INDEX", mActiveListIndex);
		prepareProgram->setUniform("TEMPLATE_COUNT", templateCount);
		prepareProgram->setUniform("TEMPLATE_CAPACITY", MaxTemplateCount);
		prepareProgram->dispatch((templateCount + mWorkGroupSize - 1u) / mWorkGroupSize);
		GL_CHECK(glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT));

		mParticlePool->bindStorage(ParticlePoolBinding);
		mActiveIndicesA->bindStorage(ActiveIndicesABinding);
		mActiveIndicesB->bindStorage(ActiveIndicesBBinding);
		mCounters->bindStorage(CountersBinding);
		GL_CHECK(glBindBufferRange(GL_SHADER_STORAGE_BUFFER, EmitterBinding, mEmitterBuffer->getBuffer(),
			static_cast<GLintptr>(mEmitterBuffer->getActiveOffset()), static_cast<GLsizeiptr>(bytes(mEmitters))));
		auto* countProgram = static_cast<ComputeProgram*>(mCompactionCountProgram.get());
		countProgram->use();
		countProgram->setUniform("ACTIVE_LIST_INDEX", mActiveListIndex);
		countProgram->setUniform("EMITTER_COUNT", uint32_t(mEmitters.size()));
		countProgram->setUniform("TEMPLATE_COUNT", templateCount);
		mCompactionDispatchCommand->bindDispatchIndirect();
		countProgram->dispatchIndirect();
		GL_CHECK(glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0));
		GL_CHECK(glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT));

		mCounters->bindStorage(CountersBinding);
		mCompactionScratch->bindStorage(CompactionScratchBinding);
		mIndirectCommands->bindStorage(IndirectCommandBinding);
		auto* prefixProgram = static_cast<ComputeProgram*>(mCompactionPrefixProgram.get());
		prefixProgram->use();
		prefixProgram->setUniform("TEMPLATE_COUNT", templateCount);
		prefixProgram->dispatch(1u);
		GL_CHECK(glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT));

		mParticlePool->bindStorage(ParticlePoolBinding);
		mRenderIndices->bindStorage(FreeIndicesBinding);
		mActiveIndicesA->bindStorage(ActiveIndicesABinding);
		mActiveIndicesB->bindStorage(ActiveIndicesBBinding);
		mCounters->bindStorage(CountersBinding);
		GL_CHECK(glBindBufferRange(GL_SHADER_STORAGE_BUFFER, EmitterBinding, mEmitterBuffer->getBuffer(),
			static_cast<GLintptr>(mEmitterBuffer->getActiveOffset()), static_cast<GLsizeiptr>(bytes(mEmitters))));
		mCompactionScratch->bindStorage(CompactionScratchBinding);
		auto* scatterProgram = static_cast<ComputeProgram*>(mCompactionScatterProgram.get());
		scatterProgram->use();
		scatterProgram->setUniform("ACTIVE_LIST_INDEX", mActiveListIndex);
		scatterProgram->setUniform("EMITTER_COUNT", uint32_t(mEmitters.size()));
		scatterProgram->setUniform("TEMPLATE_COUNT", templateCount);
		scatterProgram->setUniform("TEMPLATE_CAPACITY", MaxTemplateCount);
		mCompactionDispatchCommand->bindDispatchIndirect();
		scatterProgram->dispatchIndirect();
		GL_CHECK(glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0));
		GL_CHECK(glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT));

		mEmitterBuffer->markUsed();
	}

	void ParticleSystem::simulate()
	{
		initialise();
		if (!mAvailable) return;

		try
		{
			auto const now = chrono::steady_clock::now();
			float dt = 0.0f;
			if (mHasLastSimulationTime)
				dt = clampParticleDeltaSeconds(chrono::duration<float>(now - mLastSimulationTime).count());
			mLastSimulationTime = now;
			mHasLastSimulationTime = true;
			mSimulationSeconds += dt;

			if (!mBootstrapEmittersCreated)
			{
				createBootstrapEmitters();
				ensurePoolAllocated();
				mBootstrapEmittersCreated = true;
			}

			buildSpawnCommands(dt);
			uploadFrameData();
			{
				GpuDebugScope spawnScope("Particles: Spawn");
				dispatchSpawnCommands();
			}
			{
				GpuDebugScope simulationScope("Particles: Simulate");
				dispatchSimulation(dt);
			}
			{
				GpuDebugScope compactionScope("Particles: Compact by emitter template");
				dispatchCompaction();
			}
		}
		catch (exception const& error)
		{
			disableWithWarning(error.what());
		}
	}

	void ParticleSystem::render()
	{
		initialise();
		if (!mAvailable || !mPoolAllocated) return;

		GpuDebugScope scope("Particles: Draw");
		auto* program = static_cast<ParticleDrawProgram*>(mDrawProgram.get());
		program->use();

		mParticlePool->bindStorage(ParticlePoolBinding);
		mRenderIndices->bindStorage(FreeIndicesBinding);
		mIndirectCommands->bindDrawIndirect();

		GL_CHECK(glBindVertexArray(mVertexArray));
		GL_CHECK(glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr,
			static_cast<GLsizei>(mTemplateRenderData.size()), sizeof(ParticleDrawArraysIndirectCommand)));
		GL_CHECK(glBindVertexArray(0));
		GL_CHECK(glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0));
	}
}
