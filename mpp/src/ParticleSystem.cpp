#include <algorithm>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/gtc/type_ptr.hpp>

#include "mpp/ComputeProgram.h"
#include "mpp/GLErrorCheck.h"
#include "mpp/GpuDebugScope.h"
#include "mpp/MppException.h"
#include "mpp/ParticleDrawProgram.h"
#include "mpp/ParticleShaders.h"
#include "mpp/ParticleSystem.h"
#include "mpp/RawShaderStream.h"
#include "mpp/RenderSystem.h"
#include "mpp/RenderTexture.h"
#include "mpp/ResourceManager.h"
#include "mpp/ShaderStorageBuffer.h"
#include "mpp/Texture.h"
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
		char const* DrawProgramName = "__mpp_particle_draw__";

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

		void setTransform(EmitterSimData& emitter, glm::mat4 const& transform)
		{
			copy_n(glm::value_ptr(transform), emitter.transform.size(), emitter.transform.begin());
		}

		uint32_t nextGeneration(uint32_t generation)
		{
			++generation;
			return generation == 0 ? 1 : generation;
		}
	}

	ParticleSystem::ParticleSystem(RenderSystem* renderSystem, ResourceManager* resourceManager)
		: mwRenderSystem(renderSystem)
		, mwResourceManager(resourceManager)
	{
	}

	ParticleSystem::~ParticleSystem()
	{
		for (uint32_t index = 0; index < mTemplateTextureHandles.size(); ++index)
			releaseTemplateTextureHandle(index);
		mTemplateTextures.clear();
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
			drawStream->setDefine("MPP_PARTICLE_BINDLESS_TEXTURES", caps.supportsBindlessTextures ? "1" : "0");
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

	ParticleEmitterHandle ParticleSystem::allocateEmitter(ParticleEmitterTemplate const& emitterTemplate, glm::mat4 const& effectTransform)
	{
		uint32_t index;
		if (!mFreeEmitterIndices.empty())
		{
			index = mFreeEmitterIndices.back();
			mFreeEmitterIndices.pop_back();
		}
		else
		{
			if (mEmitterSlots.size() >= MaxEmitterCount)
				THROW_MPP("The particle emitter capacity was exceeded.", __LINE__, __FILE__, __func__);
			index = uint32_t(mEmitterSlots.size());
			mEmitterSlots.emplace_back();
			mEmitters.emplace_back();
			mTemplateRenderData.emplace_back();
			mTemplateTextures.emplace_back();
			mTemplateTextureHandles.push_back(0u);
		}

		auto& slot = mEmitterSlots[index];
		slot.occupied = true;
		slot.pendingDestroy = false;
		slot.localTransform = emitterTemplate.localTransform;
		slot.spawnAccumulator = {};
		slot.spawnCounter = 0;
		slot.burstSubmitted = false;
		slot.hasSpawned = false;
		slot.lastSpawnSeconds = mSimulationSeconds;
		slot.maximumSpawnedLifetime = 0.0f;
		mEmitters[index] = emitterTemplate.simulation;
		mEmitters[index].emissionState[3] = index;
		setTransform(mEmitters[index], effectTransform * slot.localTransform);
		mTemplateRenderData[index] = emitterTemplate.appearance;
		mTemplateTextures[index] = emitterTemplate.albedoTexture;
		return { index, slot.generation };
	}

	ParticleSystem::EmitterSlot* ParticleSystem::findEmitter(ParticleEmitterHandle handle)
	{
		if (!handle || handle.index >= mEmitterSlots.size()) return nullptr;
		auto& slot = mEmitterSlots[handle.index];
		return slot.occupied && !slot.pendingDestroy && slot.generation == handle.generation ? &slot : nullptr;
	}

	ParticleSystem::EmitterSlot const* ParticleSystem::findEmitter(ParticleEmitterHandle handle) const
	{
		if (!handle || handle.index >= mEmitterSlots.size()) return nullptr;
		auto const& slot = mEmitterSlots[handle.index];
		return slot.occupied && !slot.pendingDestroy && slot.generation == handle.generation ? &slot : nullptr;
	}

	ParticleSystem::EffectSlot* ParticleSystem::findEffect(ParticleEffectHandle handle)
	{
		if (!handle || handle.index >= mEffectSlots.size()) return nullptr;
		auto& slot = mEffectSlots[handle.index];
		return slot.occupied && slot.generation == handle.generation ? &slot : nullptr;
	}

	ParticleEffectHandle ParticleSystem::createEffect(ResourcePtr const& asset, glm::mat4 const& transform)
	{
		auto const* source = asset ? dynamic_cast<ParticleEffectSource const*>(asset.get()) : nullptr;
		if (!source) throw invalid_argument("ParticleSystem::createEffect requires a particle effect asset.");
		return createEffect(*source, transform);
	}

	ParticleEffectHandle ParticleSystem::createEffect(ParticleEffectSource const& asset, glm::mat4 const& transform)
	{
		return createEffect(asset.getEmitterTemplates(), transform);
	}

	ParticleEffectHandle ParticleSystem::createEffect(span<ParticleEmitterTemplate const> emitterTemplates, glm::mat4 const& transform)
	{
		if (emitterTemplates.empty()) return {};
		if (emitterTemplates.size() > MaxEmitterCount - getLiveEmitterCount())
			THROW_MPP("The particle emitter capacity was exceeded.", __LINE__, __FILE__, __func__);

		uint32_t effectIndex;
		if (!mFreeEffectIndices.empty())
		{
			effectIndex = mFreeEffectIndices.back();
			mFreeEffectIndices.pop_back();
		}
		else
		{
			effectIndex = uint32_t(mEffectSlots.size());
			mEffectSlots.emplace_back();
		}
		auto& effect = mEffectSlots[effectIndex];
		effect.occupied = true;
		effect.transform = transform;
		effect.emitters.clear();
		effect.emitters.reserve(emitterTemplates.size());

		try
		{
			for (auto const& emitterTemplate : emitterTemplates)
				effect.emitters.push_back(allocateEmitter(emitterTemplate, transform));
		}
		catch (...)
		{
			for (auto emitter : effect.emitters) reclaimEmitter(emitter.index);
			effect.occupied = false;
			mFreeEffectIndices.push_back(effectIndex);
			throw;
		}
		return { effectIndex, effect.generation };
	}

	void ParticleSystem::destroyEffect(ParticleEffectHandle handle)
	{
		auto* effect = findEffect(handle);
		if (!effect) return;
		for (auto emitter : effect->emitters) requestEmitterDestroy(emitter.index);
		reclaimEffect(handle.index);
	}

	void ParticleSystem::setEffectTransform(ParticleEffectHandle handle, glm::mat4 const& transform)
	{
		auto* effect = findEffect(handle);
		if (!effect) return;
		effect->transform = transform;
		for (auto emitter : effect->emitters)
		{
			auto const* slot = findEmitter(emitter);
			if (slot) setTransform(mEmitters[emitter.index], transform * slot->localTransform);
		}
	}

	void ParticleSystem::spawnEffect(ResourcePtr const& asset, glm::mat4 const& transform)
	{
		(void)createEffect(asset, transform);
	}

	void ParticleSystem::spawnEffect(ParticleEffectSource const& asset, glm::mat4 const& transform)
	{
		(void)createEffect(asset, transform);
	}

	void ParticleSystem::spawnEffect(span<ParticleEmitterTemplate const> emitterTemplates, glm::mat4 const& transform)
	{
		(void)createEffect(emitterTemplates, transform);
	}

	ParticleEmitterHandle ParticleSystem::getEmitter(ParticleEffectHandle handle, size_t emitterIndex) const
	{
		if (!handle || handle.index >= mEffectSlots.size()) return {};
		auto const& effect = mEffectSlots[handle.index];
		if (!effect.occupied || effect.generation != handle.generation || emitterIndex >= effect.emitters.size()) return {};
		auto const emitter = effect.emitters[emitterIndex];
		return findEmitter(emitter) ? emitter : ParticleEmitterHandle{};
	}

	void ParticleSystem::requestEmitterDestroy(uint32_t index)
	{
		if (index >= mEmitterSlots.size()) return;
		auto& slot = mEmitterSlots[index];
		if (!slot.occupied || slot.pendingDestroy) return;
		slot.pendingDestroy = true;
		slot.generation = nextGeneration(slot.generation);
		mEmitters[index].emissionState[1] = 0u;
		if (!slot.hasSpawned) reclaimEmitter(index);
	}

	void ParticleSystem::destroyEmitter(ParticleEmitterHandle handle)
	{
		if (!findEmitter(handle)) return;
		requestEmitterDestroy(handle.index);
	}

	void ParticleSystem::setEmitterTransform(ParticleEmitterHandle handle, glm::mat4 const& transform)
	{
		if (!findEmitter(handle)) return;
		setTransform(mEmitters[handle.index], transform);
	}

	void ParticleSystem::setEmitterParameter(ParticleEmitterHandle handle, ParticleParameter parameter, float multiplier)
	{
		if (!findEmitter(handle)) return;
		multiplier = max(0.0f, multiplier);
		auto& emitter = mEmitters[handle.index];
		switch (parameter)
		{
		case ParticleParameter::SpawnRate: emitter.parameterMultipliers0[0] = multiplier; break;
		case ParticleParameter::SizeScale: emitter.parameterMultipliers0[1] = multiplier; break;
		case ParticleParameter::SpeedScale: emitter.parameterMultipliers0[2] = multiplier; break;
		case ParticleParameter::LifetimeScale: emitter.parameterMultipliers0[3] = multiplier; break;
		case ParticleParameter::AlphaScale: emitter.parameterMultipliers1[0] = multiplier; break;
		case ParticleParameter::EmissiveScale: emitter.parameterMultipliers1[1] = multiplier; break;
		}
	}

	void ParticleSystem::startEmitter(ParticleEmitterHandle handle)
	{
		if (!findEmitter(handle)) return;
		mEmitters[handle.index].emissionState[1] = 1u;
	}

	void ParticleSystem::stopEmitter(ParticleEmitterHandle handle)
	{
		if (!findEmitter(handle)) return;
		mEmitters[handle.index].emissionState[1] = 0u;
	}

	bool ParticleSystem::isAlive(ParticleEffectHandle handle) const
	{
		if (!handle || handle.index >= mEffectSlots.size()) return false;
		auto const& effect = mEffectSlots[handle.index];
		return effect.occupied && effect.generation == handle.generation;
	}

	size_t ParticleSystem::getLiveEmitterCount() const
	{
		return count_if(mEmitterSlots.begin(), mEmitterSlots.end(), [](auto const& slot) { return slot.occupied; });
	}

	size_t ParticleSystem::getLiveEffectCount() const
	{
		return count_if(mEffectSlots.begin(), mEffectSlots.end(), [](auto const& slot) { return slot.occupied; });
	}

	bool ParticleSystem::hasOccupiedEmitters() const
	{
		return any_of(mEmitterSlots.begin(), mEmitterSlots.end(), [](auto const& slot) { return slot.occupied; });
	}

	void ParticleSystem::reclaimEmitter(uint32_t index)
	{
		if (index >= mEmitterSlots.size()) return;
		auto& slot = mEmitterSlots[index];
		if (!slot.occupied) return;
		slot.occupied = false;
		slot.pendingDestroy = false;
		slot.generation = nextGeneration(slot.generation);
		mEmitters[index] = {};
		mEmitters[index].emissionState[1] = 0u;
		mEmitters[index].emissionState[3] = index;
		releaseTemplateTextureHandle(index);
		mTemplateTextures[index].reset();
		mTemplateRenderData[index] = {};
		mFreeEmitterIndices.push_back(index);
	}

	void ParticleSystem::reclaimEffect(uint32_t index)
	{
		if (index >= mEffectSlots.size()) return;
		auto& effect = mEffectSlots[index];
		if (!effect.occupied) return;
		effect.occupied = false;
		effect.generation = nextGeneration(effect.generation);
		effect.emitters.clear();
		mFreeEffectIndices.push_back(index);
	}

	void ParticleSystem::retireCompletedEmitters()
	{
		for (uint32_t index = 0; index < mEmitterSlots.size(); ++index)
		{
			auto const& emitter = mEmitters[index];
			auto& slot = mEmitterSlots[index];
			if (!slot.occupied) continue;
			bool const oneShotComplete = emitter.emissionState[0] != 0u &&
				(slot.burstSubmitted || emitter.emissionState[1] == 0u);
			if (!(slot.pendingDestroy || oneShotComplete)) continue;
			if (!slot.hasSpawned || mSimulationSeconds - slot.lastSpawnSeconds >= slot.maximumSpawnedLifetime)
				reclaimEmitter(index);
		}

		for (uint32_t index = 0; index < mEffectSlots.size(); ++index)
		{
			auto const& effect = mEffectSlots[index];
			if (!effect.occupied) continue;
			bool const allRetired = all_of(effect.emitters.begin(), effect.emitters.end(), [this](ParticleEmitterHandle emitter)
				{ return !findEmitter(emitter); });
			if (allRetired) reclaimEffect(index);
		}
	}

	void ParticleSystem::buildSpawnCommands(float dt)
	{
		if (mEmitterSlots.size() != mEmitters.size())
			THROW_MPP("Particle emitter slots do not match the emitter table.", __LINE__, __FILE__, __func__);

		for (uint32_t emitterIndex = 0; emitterIndex < uint32_t(mEmitters.size()); ++emitterIndex)
		{
			auto const& emitter = mEmitters[emitterIndex];
			auto& slot = mEmitterSlots[emitterIndex];
			if (!slot.occupied || slot.pendingDestroy || emitter.emissionState[1] == 0u) continue;

			uint32_t spawnCount = 0;
			if (emitter.emissionState[0] == 0u)
			{
				spawnCount = slot.spawnAccumulator.accumulate(
					emitter.emissionRateAndPadding[0], emitter.parameterMultipliers0[0], dt);
			}
			else if (!slot.burstSubmitted)
			{
				spawnCount = emitter.emissionState[2];
				slot.burstSubmitted = true;
			}

			if (spawnCount == 0u) continue;
			mSpawnCommands.push_back({ emitterIndex, spawnCount, 0x9e3779b9u + emitterIndex, slot.spawnCounter });
			slot.spawnCounter += spawnCount;
			slot.hasSpawned = true;
			slot.lastSpawnSeconds = mSimulationSeconds;
			slot.maximumSpawnedLifetime = max(slot.maximumSpawnedLifetime,
				max(0.0f, emitter.lifetimeSizeRanges[1]) * max(0.0f, emitter.parameterMultipliers0[3]));
		}
	}

	void ParticleSystem::releaseTemplateTextureHandle(uint32_t templateIndex)
	{
		if (templateIndex >= mTemplateTextureHandles.size()) return;
		uint64_t const handle = mTemplateTextureHandles[templateIndex];
		if (handle == 0u) return;
		auto found = mResidentTextureHandles.find(handle);
		if (found != mResidentTextureHandles.end())
		{
			if (--found->second == 0u)
			{
				if (mwRenderSystem && mwRenderSystem->getCaps().supportsBindlessTextures)
					GL_CHECK(glMakeTextureHandleNonResidentARB(handle));
				mResidentTextureHandles.erase(found);
			}
		}
		mTemplateTextureHandles[templateIndex] = 0u;
	}

	void ParticleSystem::updateTemplateTextureHandles()
	{
		if (!mwRenderSystem->getCaps().supportsBindlessTextures) return;
		for (uint32_t index = 0; index < mTemplateTextures.size(); ++index)
		{
			auto const& resource = mTemplateTextures[index];
			if (!resource) continue;
			resource->load();
			auto* texture = dynamic_cast<Texture*>(resource.get());
			if (!texture || texture->getTextureTarget() != GL_TEXTURE_2D)
				THROW_MPP("A particle albedo atlas must be a 2D texture.", __LINE__, __FILE__, __func__);
			uint64_t const handle = glGetTextureHandleARB(texture->getId());
			if (handle == 0u)
				THROW_MPP("Could not create a bindless handle for particle atlas '" + texture->getName() + "'.", __LINE__, __FILE__, __func__);
			if (mTemplateTextureHandles[index] != handle)
			{
				releaseTemplateTextureHandle(index);
				auto [found, inserted] = mResidentTextureHandles.emplace(handle, 0u);
				if (inserted) GL_CHECK(glMakeTextureHandleResidentARB(handle));
				++found->second;
				mTemplateTextureHandles[index] = handle;
			}
			mTemplateRenderData[index].textureAndAtlas[0] = uint32_t(handle & 0xffffffffu);
			mTemplateRenderData[index].textureAndAtlas[1] = uint32_t(handle >> 32u);
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
		updateTemplateTextureHandles();

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

		try
		{
			auto const now = chrono::steady_clock::now();
			float dt = 0.0f;
			if (mHasLastSimulationTime)
				dt = clampParticleDeltaSeconds(chrono::duration<float>(now - mLastSimulationTime).count());
			mLastSimulationTime = now;
			mHasLastSimulationTime = true;
			mSimulationSeconds += dt;

			if (!hasOccupiedEmitters()) return;
			buildSpawnCommands(dt);
			if (!mAvailable)
			{
				// CPU ownership and one-shot retirement remain deterministic even on a
				// renderer where the GPU particle path is unavailable.
				mSpawnCommands.clear();
				retireCompletedEmitters();
				return;
			}
			ensurePoolAllocated();
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
			// Retirement happens only after this frame's simulation has consumed the
			// old emitter record. Reusing a slot before then could retarget a particle.
			retireCompletedEmitters();
		}
		catch (exception const& error)
		{
			disableWithWarning(error.what());
		}
	}

	void ParticleSystem::render(ParticleBlendClass blendClass, RenderTexture* sceneDepth)
	{
		initialise();
		if (!mAvailable || !mPoolAllocated) return;

		uint32_t const requestedClass = uint32_t(blendClass);
		auto const first = lower_bound(mTemplateRenderData.begin(), mTemplateRenderData.end(), requestedClass,
			[](TemplateRenderData const& appearance, uint32_t value) { return appearance.modes[3] < value; });
		auto const last = upper_bound(first, mTemplateRenderData.end(), requestedClass,
			[](uint32_t value, TemplateRenderData const& appearance) { return value < appearance.modes[3]; });
		if (first == last) return;
		size_t const firstCommand = size_t(first - mTemplateRenderData.begin());
		size_t const commandCount = size_t(last - first);

		GpuDebugScope scope("Particles: Draw blend class " + to_string(requestedClass));
		// The pass owns blend/cull/depth-test state, but depth writes are a particle
		// invariant. Force the GL state through RenderSystem so its cache remains
		// coherent when the graph scope restores authored state.
		mwRenderSystem->setDepthWriteState(false, true);
		auto* program = static_cast<ParticleDrawProgram*>(mDrawProgram.get());
		program->use();
		program->setUniform("SCENE_DEPTH", int32_t(0));
		program->setUniform("HAS_SCENE_DEPTH", int32_t(sceneDepth ? 1 : 0));
		if (sceneDepth) sceneDepth->bindDepth(0);

		mParticlePool->bindStorage(ParticlePoolBinding);
		mRenderIndices->bindStorage(FreeIndicesBinding);
		GL_CHECK(glBindBufferRange(GL_SHADER_STORAGE_BUFFER, EmitterBinding, mEmitterBuffer->getBuffer(),
			static_cast<GLintptr>(mEmitterBuffer->getActiveOffset()), static_cast<GLsizeiptr>(bytes(mEmitters))));
		GL_CHECK(glBindBufferRange(GL_SHADER_STORAGE_BUFFER, TemplateRenderBinding, mTemplateRenderBuffer->getBuffer(),
			static_cast<GLintptr>(mTemplateRenderBuffer->getActiveOffset()), static_cast<GLsizeiptr>(bytes(mTemplateRenderData))));
		mIndirectCommands->bindDrawIndirect();

		GL_CHECK(glBindVertexArray(mVertexArray));
		auto const commandOffset = reinterpret_cast<void const*>(firstCommand * sizeof(ParticleDrawArraysIndirectCommand));
		GL_CHECK(glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, commandOffset,
			static_cast<GLsizei>(commandCount), sizeof(ParticleDrawArraysIndirectCommand)));
		GL_CHECK(glBindVertexArray(0));
		GL_CHECK(glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0));
		if (sceneDepth)
		{
			GL_CHECK(glActiveTexture(GL_TEXTURE0));
			GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
		}
		mEmitterBuffer->markUsed();
		mTemplateRenderBuffer->markUsed();
	}
}
