#pragma once

#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace mpp
{
	enum class AntiAliasingSamples : uint8_t
	{
		Off,
		X2,
		X4,
		X8
	};

	inline uint32_t antiAliasingSampleCount(AntiAliasingSamples value)
	{
		switch (value)
		{
		case AntiAliasingSamples::Off: return 1;
		case AntiAliasingSamples::X2: return 2;
		case AntiAliasingSamples::X4: return 4;
		case AntiAliasingSamples::X8: return 8;
		}
		throw std::invalid_argument("Unknown anti-aliasing sample setting.");
	}

	inline float ssaaLinearScale(AntiAliasingSamples value)
	{
		return std::sqrt(static_cast<float>(antiAliasingSampleCount(value)));
	}

	inline std::string antiAliasingSamplesName(AntiAliasingSamples value)
	{
		switch (value)
		{
		case AntiAliasingSamples::Off: return "off";
		case AntiAliasingSamples::X2: return "2x";
		case AntiAliasingSamples::X4: return "4x";
		case AntiAliasingSamples::X8: return "8x";
		}
		throw std::invalid_argument("Unknown anti-aliasing sample setting.");
	}

	struct AntiAliasingDefaults
	{
		AntiAliasingSamples msaa{ AntiAliasingSamples::Off };
		AntiAliasingSamples ssaa{ AntiAliasingSamples::Off };
		bool taa{ false };
		bool fxaa{ false };
	};

	struct AntiAliasingOverrides
	{
		std::optional<AntiAliasingSamples> msaa;
		std::optional<AntiAliasingSamples> ssaa;
		std::optional<bool> taa;
		std::optional<bool> fxaa;
	};

	inline AntiAliasingDefaults resolveAntiAliasing(
		AntiAliasingDefaults const& defaults,
		AntiAliasingOverrides const& overrides)
	{
		return {
			overrides.msaa.value_or(defaults.msaa),
			overrides.ssaa.value_or(defaults.ssaa),
			overrides.taa.value_or(defaults.taa),
			overrides.fxaa.value_or(defaults.fxaa)
		};
	}

	struct RenderSystemOptions
	{
		AntiAliasingDefaults antiAliasing;
	};
}
