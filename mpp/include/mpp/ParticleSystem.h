#pragma once

#include <cstdint>
#include <memory>

#include "mpp/Config.h"
#include "mpp/Resource.h"

namespace mpp
{
	class RenderSystem;
	class ResourceManager;
	class ShaderStorageBuffer;

	// The GPU-driven particle system. The CPU manages particle effects and
	// emitters; the GPU manages particles -- nothing here reads a particle back.
	//
	// This is the vertical slice: one compute dispatch writing a particle pool
	// and its own indirect draw arguments, and one attribute-less indirect draw
	// of untextured quads. Emitters, spawn commands, behaviour modules,
	// compaction and appearances all arrive later against these seams.
	//
	// Simulation runs once per rendered frame, before graph execution (ADR 0005).
	// Drawing happens inside MPP.ParticleScene, which is a pure draw pass and may
	// execute several times per frame -- a planar reflection or a point-shadow
	// face expansion must not advance the simulation.
	class _MPPAPI ParticleSystem
	{
	public:

		// One vec4 per particle for now, so the pool is 64 KB. The real pool
		// sizing arrives with the free list.
		static constexpr uint32_t ParticleCount = 4096;

	private:

		RenderSystem* mwRenderSystem;

		ResourceManager* mwResourceManager;

		ResourcePtr mSimulationProgram, mDrawProgram;

		std::unique_ptr<ShaderStorageBuffer> mParticlePool, mIndirectCommands;

		uint32_t mVertexArray{ 0 };

		uint32_t mWorkGroupSize{ 64 };

		bool mInitialised{ false };

		bool mAvailable{ false };

	public:

		ParticleSystem(RenderSystem* renderSystem, ResourceManager* resourceManager);

		~ParticleSystem();

		ParticleSystem(ParticleSystem const&) = delete;

		ParticleSystem& operator =(ParticleSystem const&) = delete;

		// Creates the GPU resources on first use. Missing compute support, or a
		// driver that refuses an otherwise valid kernel, is not fatal: the system
		// warns exactly once and then draws nothing, following the incomplete
		// PBR-environment precedent rather than the throwing SSAA one.
		void initialise();

		bool isAvailable() const { return mAvailable; }

		// One dispatch per rendered frame. Safe to call when unavailable.
		void simulate();

		// The indirect draw. Safe to call when unavailable, and before the first
		// simulate: the command buffer starts zeroed, which is an empty draw.
		void render();
	};
}
