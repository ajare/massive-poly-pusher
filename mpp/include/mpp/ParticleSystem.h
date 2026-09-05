#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "mpp/Config.h"
#include "mpp/ParticleData.h"
#include "mpp/Resource.h"

namespace mpp
{
	class RenderSystem;
	class ResourceManager;
	class ShaderStorageBuffer;
	namespace detail { class PersistentMappedBuffer; }

	// The GPU-driven particle system. CPU state consists only of emitter records
	// and spawn commands; particles and their allocation state stay on the GPU.
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
		std::vector<ParticleSpawnCommand> mSpawnCommands;
		struct EmitterFrameState
		{
			ParticleSpawnAccumulator spawnAccumulator;
			uint32_t spawnCounter{ 0 };
			bool burstSubmitted{ false };
		};
		std::vector<EmitterFrameState> mEmitterFrameStates;

		uint32_t mVertexArray{ 0 };
		uint32_t mNoiseTexture{ 0 };
		uint32_t mWorkGroupSize{ 64 };
		uint32_t mPoolCapacity{ 0 };
		uint32_t mActiveListIndex{ 0 };
		bool mInitialised{ false };
		bool mAvailable{ false };
		bool mPoolAllocated{ false };
		bool mBootstrapEmittersCreated{ false };
		bool mHasLastSimulationTime{ false };
		std::chrono::steady_clock::time_point mLastSimulationTime{};
		float mSimulationSeconds{ 0.0f };

		void ensurePoolAllocated();
		void createNoiseTexture();
		void createBootstrapEmitters();
		void buildSpawnCommands(float dt);
		void uploadFrameData();
		void dispatchSpawnCommands();
		void dispatchSimulation(float dt);
		void dispatchCompaction();
		void disableWithWarning(std::string const& reason);

	public:
		ParticleSystem(RenderSystem* renderSystem, ResourceManager* resourceManager);
		~ParticleSystem();
		ParticleSystem(ParticleSystem const&) = delete;
		ParticleSystem& operator =(ParticleSystem const&) = delete;

		// Compiles the kernels and draw program, but deliberately does not allocate
		// the pool. Pool allocation starts only when the first emitter exists.
		void initialise();

		bool isAvailable() const { return mAvailable; }
		bool isPoolAllocated() const { return mPoolAllocated; }
		uint32_t getPoolCapacity() const { return mPoolCapacity; }

		// Spawn preparation and simulation are called once per rendered frame outside
		// graph passes. RenderSystem guards the frame serial before entering here.
		void simulate();

		// Draws contiguous per-template ranges from the render index list in one
		// GPU-authored indirect multi-draw for the current additive blend class.
		// Safe before pool allocation and when compute is unavailable.
		void render();
	};
}
