#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include <glm/vec3.hpp>

#include <mpp/ParticleEffectBounds.h>

namespace particle_editor
{
	enum class PreviewGraph : uint32_t
	{
		Pbr,
		Legacy
	};

	enum class StudioPreset : uint32_t
	{
		Neutral,
		Dark,
		Warm,
		Count
	};

	enum StudioFace : uint32_t
	{
		StudioFloor = 1u << 0,
		StudioCeiling = 1u << 1,
		StudioLeft = 1u << 2,
		StudioRight = 1u << 3,
		StudioBack = 1u << 4,
		StudioFront = 1u << 5
	};

	struct StudioVolume
	{
		glm::vec3 center{ 0.0f, 2.0f, 0.0f };
		glm::vec3 size{ 12.0f, 8.0f, 12.0f };
	};

	struct ParticlePreviewPreferences
	{
		PreviewGraph graph{ PreviewGraph::Pbr };
		glm::vec3 cameraTarget{ 0.0f, 1.5f, 0.0f };
		float cameraYaw{ 0.0f };
		float cameraPitch{ 0.28f };
		float cameraDistance{ 7.5f };
		StudioPreset studioPreset{ StudioPreset::Neutral };
		bool floorGrid{ true };
		bool lightEnabled{ true };
		float lightAzimuth{ -0.65f };
		float lightElevation{ 0.72f };
		float lightDistance{ 6.0f };
		glm::vec3 lightColour{ 1.0f, 0.92f, 0.8f };
		float lightIntensity{ 3.0f };
		bool lightAutoOrbit{ false };
		float lightAutoOrbitSpeed{ 0.35f };
	};

	StudioVolume studioVolumeForBounds(std::optional<mpp::ParticleEffectBounds> const& bounds);
	uint32_t obstructingStudioFaces(StudioVolume const& studio, glm::vec3 const& cameraPosition);
	glm::vec3 previewLightPosition(ParticlePreviewPreferences const& preferences, StudioVolume const& studio);
	ParticlePreviewPreferences loadParticlePreviewPreferences(std::filesystem::path const& path);
	void saveParticlePreviewPreferences(std::filesystem::path const& path,
		ParticlePreviewPreferences const& preferences);
	bool runParticlePreviewPreferenceTests(std::string* failure = nullptr);
}
