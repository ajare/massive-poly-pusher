#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

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

	enum class ParticleBehaviourModule : uint32_t
	{
		Gravity = 1u << 0,
		Drag = 1u << 1,
		Noise = 1u << 2
	};

	// Particle appearance enums are stored directly in TemplateRenderData::modes.
	// Animation playback occupies the low byte; RandomStart is an independent bit
	// so it can be combined with either playback mode.
	enum class ParticleBillboardMode : uint32_t
	{
		CameraFacing,
		ScreenAligned,
		Cylindrical,
		AxisLocked,
		VelocityAligned
	};

	enum class ParticleTextureAnimation : uint32_t
	{
		None = 0u,
		FrameOverLife = 1u,
		FixedRate = 2u,
		RandomStart = 1u << 8
	};

	constexpr ParticleTextureAnimation operator |(ParticleTextureAnimation left, ParticleTextureAnimation right) noexcept
	{
		return ParticleTextureAnimation(uint32_t(left) | uint32_t(right));
	}

	enum class ParticleBlendClass : uint32_t
	{
		Additive,
		Alpha
	};

	inline constexpr uint32_t ParticleTexturePlaybackMask = 0xffu;
	inline constexpr uint32_t ParticleTextureRandomStartBit = uint32_t(ParticleTextureAnimation::RandomStart);
	inline constexpr float MaximumParticleDeltaSeconds = 0.1f;

	// Scalar slots are packed generically four channels per LUT row. Keep the
	// authored semantics here so adding a slot only changes this assignment; the
	// baker itself is independent of the number and meaning of curves.
	enum class ParticleScalarCurve : uint32_t
	{
		Size,
		Alpha,
		VelocityMultiplier,
		Drag,
		RotationSpeed,
		EmissiveIntensity,
		Count
	};

	struct ParticleCurveKey
	{
		float time{ 0.0f };
		float value{ 1.0f };
	};

	struct ParticleCurve
	{
		float defaultValue{ 1.0f };
		std::vector<ParticleCurveKey> keys;
	};

	struct ParticleGradientKey
	{
		float time{ 0.0f };
		std::array<float, 3> colour{ 1.0f, 1.0f, 1.0f };
	};

	struct ParticleGradient
	{
		std::array<float, 3> defaultColour{ 1.0f, 1.0f, 1.0f };
		std::vector<ParticleGradientKey> keys;
	};

	constexpr float clampParticleDeltaSeconds(float seconds) noexcept
	{
		return seconds <= 0.0f ? 0.0f :
			(seconds > MaximumParticleDeltaSeconds ? MaximumParticleDeltaSeconds : seconds);
	}

	// CPU reference for the shader's flipbook selection. This is also useful to
	// asset previews that need to show the exact frame the runtime will choose.
	inline uint32_t particleFlipbookFrame(uint32_t frameCount, uint32_t animation,
		float age, float lifetime, float fixedRate, uint32_t seed) noexcept
	{
		frameCount = frameCount == 0u ? 1u : frameCount;
		uint32_t frame = 0u;
		auto const playback = animation & ParticleTexturePlaybackMask;
		if (playback == uint32_t(ParticleTextureAnimation::FrameOverLife) && lifetime > 0.0f)
		{
			float const normalizedAge = age <= 0.0f ? 0.0f : (age >= lifetime ? 1.0f : age / lifetime);
			frame = normalizedAge >= 1.0f ? frameCount - 1u : uint32_t(normalizedAge * float(frameCount));
		}
		else if (playback == uint32_t(ParticleTextureAnimation::FixedRate) && age > 0.0f && fixedRate > 0.0f)
		{
			frame = uint32_t(age * fixedRate) % frameCount;
		}
		if ((animation & ParticleTextureRandomStartBit) != 0u) frame = (frame + seed % frameCount) % frameCount;
		return frame;
	}

	// CPU-side state for one continuous emitter. Keeping the fractional part is
	// what makes rates below one particle per frame deterministic and smooth.
	struct ParticleSpawnAccumulator
	{
		double remainder{ 0.0 };

		uint32_t accumulate(float particlesPerSecond, float rateMultiplier, float dt) noexcept
		{
			if (!(particlesPerSecond > 0.0f) || !(rateMultiplier > 0.0f) || !(dt > 0.0f)) return 0u;
			double const accumulated = remainder + double(particlesPerSecond) * double(rateMultiplier) * double(dt);
			if (accumulated >= double(UINT32_MAX))
			{
				remainder = 0.0;
				return UINT32_MAX;
			}
			uint32_t const whole = uint32_t(accumulated);
			remainder = accumulated - double(whole);
			return whole;
		}
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
		// Emission mode (continuous/burst), enabled, burst count, emitter-template index.
		std::array<uint32_t, 4> emissionState{ 0u, 1u, 0u, 0u };
		// Authored continuous spawn rate in particles/second, then padding.
		std::array<float, 4> emissionRateAndPadding{};
		// Spawn-rate, size, speed, and lifetime multipliers.
		std::array<float, 4> parameterMultipliers0{ 1.0f, 1.0f, 1.0f, 1.0f };
		// Alpha and emissive multipliers, then padding.
		std::array<float, 4> parameterMultipliers1{ 1.0f, 1.0f, 0.0f, 0.0f };
		// Fixed behaviour-module slots consumed by the simulation kernel.
		std::array<float, 4> gravityAndDrag{};
		std::array<float, 4> noiseFrequencyStrength{};
		std::array<float, 4> noiseScrollAndTimeScale{};
	};

	// Draw-only emitter-template data. This remains a compact 64-byte std430
	// record and is not fetched by the spawn or simulation kernels.
	struct alignas(16) TemplateRenderData
	{
		// Bindless albedo handle low/high words, atlas columns, atlas rows. A
		// zero handle is the declared white fallback on contexts without bindless
		// textures and for appearances that intentionally omit an albedo texture.
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

	// Header of the GPU counter buffer. Per-template live counts immediately
	// follow this header. Compaction rebuilds those counts, spawning consumes
	// them for the template budget, and the statistics path copies this same
	// buffer rather than maintaining another set of counters.
	struct alignas(16) ParticleCounterHeader
	{
		uint32_t freeCount{ 0 };
		uint32_t activeCountA{ 0 };
		uint32_t activeCountB{ 0 };
		uint32_t droppedSpawnCount{ 0 };
	};

	// Binary layout required by glMultiDrawArraysIndirect. first encodes the
	// template's render-list offset as offset * 4, allowing GLSL 4.30 to recover
	// the range without requiring ARB_shader_draw_parameters.
	struct ParticleDrawArraysIndirectCommand
	{
		uint32_t count{ 4 };
		uint32_t instanceCount{ 0 };
		uint32_t first{ 0 };
		uint32_t baseInstance{ 0 };
	};

	static_assert(std::is_standard_layout_v<ParticleRecord>);
	static_assert(offsetof(ParticleRecord, positionAge) == 0);
	static_assert(offsetof(ParticleRecord, velocityLifetime) == 16);
	static_assert(offsetof(ParticleRecord, packedColour) == 32);
	static_assert(offsetof(ParticleRecord, emitterIndex) == 48);
	static_assert(offsetof(ParticleRecord, padding) == 60);
	static_assert(sizeof(ParticleRecord) == 64, "The std430 particle array stride must be exactly 64 bytes.");
	static_assert(clampParticleDeltaSeconds(3.0f) == MaximumParticleDeltaSeconds);
	static_assert(sizeof(EmitterSimData) == 304);
	static_assert(sizeof(TemplateRenderData) == 64);
	static_assert(sizeof(ParticleSpawnCommand) == 16);
	static_assert(sizeof(ParticleCounterHeader) == 16);
	static_assert(sizeof(ParticleDrawArraysIndirectCommand) == 16);
}
