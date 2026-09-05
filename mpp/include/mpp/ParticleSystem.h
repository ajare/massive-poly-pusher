#pragma once

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
		static constexpr uint32_t MaxSpawnCommandCount = 4096;

	private:
		RenderSystem* mwRenderSystem;
		ResourceManager* mwResourceManager;

		ResourcePtr mPoolInitialiseProgram, mSpawnProgram, mDrawProgram;

		std::unique_ptr<ShaderStorageBuffer> mParticlePool;
		std::unique_ptr<ShaderStorageBuffer> mFreeIndices;
		std::unique_ptr<ShaderStorageBuffer> mActiveIndicesA;
		std::unique_ptr<ShaderStorageBuffer> mActiveIndicesB;
		std::unique_ptr<ShaderStorageBuffer> mCounters;
		std::unique_ptr<ShaderStorageBuffer> mIndirectCommands;
		std::unique_ptr<detail::PersistentMappedBuffer> mEmitterBuffer;
		std::unique_ptr<detail::PersistentMappedBuffer> mTemplateRenderBuffer;
		std::unique_ptr<detail::PersistentMappedBuffer> mSpawnCommandBuffer;

		std::vector<EmitterSimData> mEmitters;
		std::vector<TemplateRenderData> mTemplateRenderData;
		std::vector<ParticleSpawnCommand> mSpawnCommands;

		uint32_t mVertexArray{ 0 };
		uint32_t mWorkGroupSize{ 64 };
		uint32_t mPoolCapacity{ 0 };
		uint32_t mActiveListIndex{ 0 };
		bool mInitialised{ false };
		bool mAvailable{ false };
		bool mPoolAllocated{ false };
		bool mBootstrapEmittersCreated{ false };

		void ensurePoolAllocated();
		void createBootstrapEmitters();
		void uploadAndDispatchSpawnCommands();
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

		// Spawn preparation is called once per rendered frame outside graph passes.
		// This milestone has no integration kernel yet; that follows in #18.
		void simulate();

		// Draws only indices in the current active list, using GPU-authored indirect
		// arguments. Safe before pool allocation and when compute is unavailable.
		void render();
	};
}
