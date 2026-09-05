#pragma once

#include <chrono>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>

#include "mpp/Config.h"
#include "mpp/ParticleData.h"
#include "mpp/Resource.h"

namespace mpp
{
	class RenderSystem;
	class ResourceManager;
	class ShaderStorageBuffer;
	class RenderTexture;
	class ParticleEffectCurveLut;
	namespace detail { class PersistentMappedBuffer; }

	enum class ParticleParameter : uint32_t
	{
		SpawnRate,
		SizeScale,
		SpeedScale,
		LifetimeScale,
		AlphaScale,
		EmissiveScale
	};

	struct _MPPAPI ParticleEmitterHandle
	{
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();
		uint32_t index{ InvalidIndex };
		uint32_t generation{ 0 };
		explicit operator bool() const noexcept { return index != InvalidIndex && generation != 0; }
		auto operator<=>(ParticleEmitterHandle const&) const = default;
	};

	struct _MPPAPI ParticleEffectHandle
	{
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();
		uint32_t index{ InvalidIndex };
		uint32_t generation{ 0 };
		explicit operator bool() const noexcept { return index != InvalidIndex && generation != 0; }
		auto operator<=>(ParticleEffectHandle const&) const = default;
	};

	// Runtime form of one authored emitter template. Asset implementations copy
	// their authored values into this structure; ParticleSystem then owns each
	// live emitter's mutable copy.
	struct _MPPAPI ParticleEmitterTemplate
	{
		EmitterSimData simulation{};
		TemplateRenderData appearance{};
		// The atlas resource belongs to the emitter template. ParticleSystem turns
		// it into the bindless handle stored in appearance at upload time.
		ResourcePtr albedoTexture;
		std::array<ParticleCurve, size_t(ParticleScalarCurve::Count)> curves{};
		ParticleGradient colourGradient{};
		glm::mat4 localTransform{ 1.0f };
	};

	// Narrow bridge between the CPU API and the particle effect Resource supplied
	// by the asset ticket. Keeping it abstract lets this API remain usable by
	// programmatic effects without taking ownership of parsing or authoring.
	class _MPPAPI ParticleEffectSource
	{
		mutable std::shared_ptr<ParticleEffectCurveLut> mCurveLut;

	protected:
		// Asset streams call this if authored templates are replaced before reload.
		void invalidateCurveLut() const;

	public:
		virtual ~ParticleEffectSource() = default;
		virtual std::span<ParticleEmitterTemplate const> getEmitterTemplates() const = 0;
		// The first request performs the load-time CPU bake. The strong cache makes
		// one LUT belong to this reusable particle effect asset.
		std::shared_ptr<ParticleEffectCurveLut> getCurveLut() const;
	};

	// The GPU-driven particle system. The CPU owns effects, generational emitter
	// slots and spawn commands; particles and their allocation state stay on GPU.
	class _MPPAPI ParticleSystem
	{
	public:
		static constexpr uint32_t MaxEmitterCount = 4096;
		static constexpr uint32_t MaxTemplateCount = 4096;
		static constexpr uint32_t MaxSpawnCommandCount = 4096;

	private:
		RenderSystem* mwRenderSystem;
		ResourceManager* mwResourceManager;

		ResourcePtr mPoolInitialiseProgram, mSpawnProgram, mSimulationPrepareProgram, mSimulationProgram;
		ResourcePtr mCompactionPrepareProgram, mCompactionCountProgram, mCompactionPrefixProgram, mCompactionScatterProgram;
		ResourcePtr mDrawProgram;

		std::unique_ptr<ShaderStorageBuffer> mParticlePool;
		std::unique_ptr<ShaderStorageBuffer> mFreeIndices;
		std::unique_ptr<ShaderStorageBuffer> mActiveIndicesA;
		std::unique_ptr<ShaderStorageBuffer> mActiveIndicesB;
		std::unique_ptr<ShaderStorageBuffer> mRenderIndices;
		std::unique_ptr<ShaderStorageBuffer> mCounters;
		std::unique_ptr<ShaderStorageBuffer> mCompactionScratch;
		std::unique_ptr<ShaderStorageBuffer> mIndirectCommands;
		std::unique_ptr<ShaderStorageBuffer> mSimulationDispatchCommand;
		std::unique_ptr<ShaderStorageBuffer> mCompactionDispatchCommand;
		std::unique_ptr<detail::PersistentMappedBuffer> mEmitterBuffer;
		std::unique_ptr<detail::PersistentMappedBuffer> mTemplateRenderBuffer;
		std::unique_ptr<detail::PersistentMappedBuffer> mSpawnCommandBuffer;

		std::vector<EmitterSimData> mEmitters;
		std::vector<TemplateRenderData> mTemplateRenderData;
		std::vector<ResourcePtr> mTemplateTextures;
		std::vector<std::shared_ptr<ParticleEffectCurveLut>> mTemplateCurveLuts;
		std::vector<uint64_t> mTemplateTextureHandles;
		std::map<uint64_t, uint32_t> mResidentTextureHandles;
		std::vector<ParticleSpawnCommand> mSpawnCommands;
		struct EmitterSlot
		{
			uint32_t generation{ 1 };
			bool occupied{ false };
			bool pendingDestroy{ false };
			glm::mat4 localTransform{ 1.0f };
			ParticleSpawnAccumulator spawnAccumulator;
			uint32_t spawnCounter{ 0 };
			bool burstSubmitted{ false };
			bool hasSpawned{ false };
			float lastSpawnSeconds{ 0.0f };
			float maximumSpawnedLifetime{ 0.0f };
		};
		struct EffectSlot
		{
			uint32_t generation{ 1 };
			bool occupied{ false };
			glm::mat4 transform{ 1.0f };
			std::vector<ParticleEmitterHandle> emitters;
			ResourcePtr asset;
			std::shared_ptr<ParticleEffectCurveLut> curveLut;
		};
		std::vector<EmitterSlot> mEmitterSlots;
		std::vector<uint32_t> mFreeEmitterIndices;
		std::vector<EffectSlot> mEffectSlots;
		std::vector<uint32_t> mFreeEffectIndices;

		uint32_t mVertexArray{ 0 };
		uint32_t mNoiseTexture{ 0 };
		uint32_t mWorkGroupSize{ 64 };
		uint32_t mPoolCapacity{ 0 };
		uint32_t mActiveListIndex{ 0 };
		bool mInitialised{ false };
		bool mAvailable{ false };
		bool mPoolAllocated{ false };
		bool mHasLastSimulationTime{ false };
		std::chrono::steady_clock::time_point mLastSimulationTime{};
		float mSimulationSeconds{ 0.0f };

		void ensurePoolAllocated();
		void createNoiseTexture();
		void buildSpawnCommands(float dt);
		void retireCompletedEmitters();
		void uploadFrameData();
		void updateTemplateTextureHandles();
		void releaseTemplateTextureHandle(uint32_t templateIndex);
		void dispatchSpawnCommands();
		void dispatchSimulation(float dt);
		void dispatchCompaction();
		void disableWithWarning(std::string const& reason);
		ParticleEmitterHandle allocateEmitter(ParticleEmitterTemplate const& emitterTemplate, glm::mat4 const& effectTransform,
			std::shared_ptr<ParticleEffectCurveLut> const& curveLut, size_t emitterTemplateIndex);
		ParticleEffectHandle createEffect(std::span<ParticleEmitterTemplate const> emitterTemplates, glm::mat4 const& transform,
			std::shared_ptr<ParticleEffectCurveLut> curveLut);
		EmitterSlot* findEmitter(ParticleEmitterHandle handle);
		EmitterSlot const* findEmitter(ParticleEmitterHandle handle) const;
		EffectSlot* findEffect(ParticleEffectHandle handle);
		void requestEmitterDestroy(uint32_t index);
		void reclaimEmitter(uint32_t index);
		void reclaimEffect(uint32_t index);
		bool hasOccupiedEmitters() const;
		friend _MPPAPI bool runParticleSystemCpuTests(std::string* failure);
		friend _MPPAPI bool runParticleGpuTests(RenderSystem* renderSystem, std::string* failure);

	public:
		ParticleSystem(RenderSystem* renderSystem, ResourceManager* resourceManager);
		~ParticleSystem();
		ParticleSystem(ParticleSystem const&) = delete;
		ParticleSystem& operator =(ParticleSystem const&) = delete;

		void initialise();
		bool isAvailable() const { return mAvailable; }
		bool isPoolAllocated() const { return mPoolAllocated; }
		uint32_t getPoolCapacity() const { return mPoolCapacity; }

		ParticleEffectHandle createEffect(ResourcePtr const& asset, glm::mat4 const& transform = glm::mat4(1.0f));
		ParticleEffectHandle createEffect(ParticleEffectSource const& asset, glm::mat4 const& transform = glm::mat4(1.0f));
		ParticleEffectHandle createEffect(std::span<ParticleEmitterTemplate const> emitterTemplates, glm::mat4 const& transform = glm::mat4(1.0f));
		void destroyEffect(ParticleEffectHandle effect);
		void setEffectTransform(ParticleEffectHandle effect, glm::mat4 const& transform);
		void spawnEffect(ResourcePtr const& asset, glm::mat4 const& transform = glm::mat4(1.0f));
		void spawnEffect(ParticleEffectSource const& asset, glm::mat4 const& transform = glm::mat4(1.0f));
		void spawnEffect(std::span<ParticleEmitterTemplate const> emitterTemplates, glm::mat4 const& transform = glm::mat4(1.0f));

		ParticleEmitterHandle getEmitter(ParticleEffectHandle effect, size_t index) const;
		void destroyEmitter(ParticleEmitterHandle emitter);
		void setEmitterTransform(ParticleEmitterHandle emitter, glm::mat4 const& transform);
		void setEmitterParameter(ParticleEmitterHandle emitter, ParticleParameter parameter, float multiplier);
		void startEmitter(ParticleEmitterHandle emitter);
		void stopEmitter(ParticleEmitterHandle emitter);
		bool isAlive(ParticleEmitterHandle emitter) const { return findEmitter(emitter) != nullptr; }
		bool isAlive(ParticleEffectHandle effect) const;
		size_t getLiveEmitterCount() const;
		size_t getLiveEffectCount() const;

		// Spawn preparation and simulation are called once per rendered frame outside
		// graph passes. RenderSystem guards the frame serial before entering here.
		void simulate();
		// Draw only the contiguous command span selected by this authored pass.
		// A null scene depth selects hard-edged particles.
		void render(ParticleBlendClass blendClass, RenderTexture* sceneDepth);
	};
}
