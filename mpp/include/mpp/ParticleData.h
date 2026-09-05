#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace mpp
{
	enum class ParticleSpawnShape : uint32_t
	{
		Point,
		Line,
		Box,
		Sphere,
		Hemisphere,
		Disc,
		Cone
	};

	// CPU mirror of the std430 particle record. vec3-plus-float values are
	// represented as four scalars so the layout does not depend on GLM alignment
	// options. The matching GLSL declaration lives in ParticleShaders.h.
	struct alignas(16) ParticleRecord
	{
		std::array<float, 4> positionAge{};
		std::array<float, 4> velocityLifetime{};
		uint32_t packedColour{ 0xffffffffu };
		float baseSize{ 1.0f };
		float rotation{ 0.0f };
		float angularVelocity{ 0.0f };
		uint32_t emitterIndex{ 0 };
		uint32_t seed{ 0 };
		uint32_t flags{ 0 };
		uint32_t padding{ 0 };
	};

	// Simulation and spawn data are deliberately separate from appearance data:
	// the simulation kernel dereferences this record for every active particle
	// and never needs a texture handle, atlas mode, or blend class.
	struct alignas(16) EmitterSimData
	{
		// Column-major world transform.
		std::array<float, 16> transform{
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
		// Line half-vector, box half-extents, or radius/height for radial shapes.
		std::array<float, 4> shapeParameters{};
		std::array<float, 4> initialVelocityMin{};
		std::array<float, 4> initialVelocityMax{};
		std::array<float, 4> colourMin{ 1.0f, 1.0f, 1.0f, 1.0f };
		std::array<float, 4> colourMax{ 1.0f, 1.0f, 1.0f, 1.0f };
		// lifetime min/max, base-size min/max.
		std::array<float, 4> lifetimeSizeRanges{ 1.0f, 1.0f, 1.0f, 1.0f };
		// rotation min/max, angular-velocity min/max.
		std::array<float, 4> rotationRanges{};
		// Spawn shape, emitter seed, behaviour-module mask, particle budget.
		std::array<uint32_t, 4> shapeSeedModulesBudget{};
		// Emission mode, enabled, burst count, reserved.
		std::array<uint32_t, 4> emissionState{ 0u, 1u, 0u, 0u };
		// Spawn-rate, size, speed, and lifetime multipliers.
		std::array<float, 4> parameterMultipliers0{ 1.0f, 1.0f, 1.0f, 1.0f };
		// Alpha and emissive multipliers, then padding.
		std::array<float, 4> parameterMultipliers1{ 1.0f, 1.0f, 0.0f, 0.0f };
		// Fixed module slots used by the next simulation milestone.
		std::array<float, 4> gravityAndDrag{};
		std::array<float, 4> noiseFrequencyStrength{};
		std::array<float, 4> noiseScrollAndTimeScale{};
	};

	// Draw-only emitter-template data. This remains a compact 64-byte std430
	// record and is not fetched by the spawn or simulation kernels.
	struct alignas(16) TemplateRenderData
	{
		// Bindless albedo handle low/high words, atlas columns, atlas rows.
		std::array<uint32_t, 4> textureAndAtlas{ 0u, 0u, 1u, 1u };
		// RGB tint and alpha multiplier.
		std::array<float, 4> tintAndAlpha{ 1.0f, 1.0f, 1.0f, 1.0f };
		// Emissive intensity, soft fade distance, animation rate, LUT row.
		std::array<float, 4> appearance{ 1.0f, 0.0f, 0.0f, 0.0f };
		// Frame count, animation mode, billboard mode, blend class.
		std::array<uint32_t, 4> modes{ 1u, 0u, 0u, 0u };
	};

	// One CPU-to-GPU spawn command. spawnCounter is the first logical spawn
	// number represented by this command; no counter is stored per particle.
	struct alignas(16) ParticleSpawnCommand
	{
		uint32_t emitterIndex{ 0 };
		uint32_t count{ 0 };
		uint32_t randomSeed{ 0 };
		uint32_t spawnCounter{ 0 };
	};

	// Header of the GPU counter buffer. Per-emitter live counts immediately
	// follow this header in the same SSBO.
	struct alignas(16) ParticleCounterHeader
	{
		uint32_t freeCount{ 0 };
		uint32_t activeCountA{ 0 };
		uint32_t activeCountB{ 0 };
		uint32_t droppedSpawnCount{ 0 };
	};

	static_assert(std::is_standard_layout_v<ParticleRecord>);
	static_assert(offsetof(ParticleRecord, positionAge) == 0);
	static_assert(offsetof(ParticleRecord, velocityLifetime) == 16);
	static_assert(offsetof(ParticleRecord, packedColour) == 32);
	static_assert(offsetof(ParticleRecord, emitterIndex) == 48);
	static_assert(offsetof(ParticleRecord, padding) == 60);
	static_assert(sizeof(ParticleRecord) == 64, "The std430 particle array stride must be exactly 64 bytes.");
	static_assert(sizeof(EmitterSimData) == 288);
	static_assert(sizeof(TemplateRenderData) == 64);
	static_assert(sizeof(ParticleSpawnCommand) == 16);
	static_assert(sizeof(ParticleCounterHeader) == 16);
}
