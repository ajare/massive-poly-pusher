#include "ParticlePreviewPreferences.h"

#include "ParticleDocument.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>

#include <glm/common.hpp>

#include <mpp/resource-parsers/ParticleEffectSerializer.h>
#include <system_error>

namespace particle_editor
{
	namespace
	{
		StudioVolume const FallbackStudio{};

		bool finite(glm::vec3 const& value)
		{
			return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
		}

		bool readFloat(std::string const& text, float& value)
		{
			try
			{
				size_t consumed = 0;
				float candidate = std::stof(text, &consumed);
				if (consumed != text.size() || !std::isfinite(candidate)) return false;
				value = candidate;
				return true;
			}
			catch (...) { return false; }
		}

		bool readVec3(std::string const& text, glm::vec3& value)
		{
			std::istringstream input(text);
			char first = 0, second = 0;
			glm::vec3 candidate;
			if (!(input >> candidate.x >> first >> candidate.y >> second >> candidate.z) ||
				first != ',' || second != ',' || !finite(candidate)) return false;
			input >> std::ws;
			if (!input.eof()) return false;
			value = candidate;
			return true;
		}

		bool readBool(std::string const& text, bool& value)
		{
			if (text == "true" || text == "1") { value = true; return true; }
			if (text == "false" || text == "0") { value = false; return true; }
			return false;
		}

		void sanitize(ParticlePreviewPreferences& value)
		{
			if (!finite(value.cameraTarget)) value.cameraTarget = { 0.0f, 1.5f, 0.0f };
			value.cameraYaw = std::isfinite(value.cameraYaw) ? value.cameraYaw : 0.0f;
			value.cameraPitch = std::clamp(std::isfinite(value.cameraPitch) ? value.cameraPitch : 0.28f, -1.5f, 1.5f);
			value.cameraDistance = std::clamp(std::isfinite(value.cameraDistance) ? value.cameraDistance : 7.5f, 0.05f, 100000.0f);
			if (uint32_t(value.studioPreset) >= uint32_t(StudioPreset::Count)) value.studioPreset = StudioPreset::Neutral;
			value.lightAzimuth = std::isfinite(value.lightAzimuth) ? value.lightAzimuth : -0.65f;
			value.lightElevation = std::clamp(std::isfinite(value.lightElevation) ? value.lightElevation : 0.72f, -1.5f, 1.5f);
			value.lightDistance = std::clamp(std::isfinite(value.lightDistance) ? value.lightDistance : 6.0f, 0.05f, 100000.0f);
			if (!finite(value.lightColour)) value.lightColour = { 1.0f, 0.92f, 0.8f };
			value.lightColour = glm::max(value.lightColour, glm::vec3(0.0f));
			value.lightIntensity = std::clamp(std::isfinite(value.lightIntensity) ? value.lightIntensity : 3.0f, 0.0f, 100000.0f);
			value.lightAutoOrbitSpeed = std::clamp(std::isfinite(value.lightAutoOrbitSpeed) ? value.lightAutoOrbitSpeed : 0.35f, -20.0f, 20.0f);
		}

		bool nearlyEqual(float left, float right)
		{
			return std::abs(left - right) <= 0.00001f;
		}
	}

	StudioVolume studioVolumeForBounds(std::optional<mpp::ParticleEffectBounds> const& bounds)
	{
		if (!bounds || !finite(bounds->center) || !finite(bounds->size) ||
			bounds->size.x <= 0.0f || bounds->size.y <= 0.0f || bounds->size.z <= 0.0f)
			return FallbackStudio;
		return { bounds->center, bounds->size * 1.5f };
	}

	uint32_t obstructingStudioFaces(StudioVolume const& studio, glm::vec3 const& cameraPosition)
	{
		auto const minimum = studio.center - studio.size * 0.5f;
		auto const maximum = studio.center + studio.size * 0.5f;
		uint32_t result = 0;
		if (cameraPosition.x < minimum.x) result |= StudioLeft;
		if (cameraPosition.x > maximum.x) result |= StudioRight;
		if (cameraPosition.y < minimum.y) result |= StudioFloor;
		if (cameraPosition.y > maximum.y) result |= StudioCeiling;
		if (cameraPosition.z < minimum.z) result |= StudioBack;
		if (cameraPosition.z > maximum.z) result |= StudioFront;
		return result;
	}

	glm::vec3 previewLightPosition(ParticlePreviewPreferences const& preferences, StudioVolume const& studio)
	{
		float const cosine = std::cos(preferences.lightElevation);
		return studio.center + preferences.lightDistance * glm::vec3(
			std::sin(preferences.lightAzimuth) * cosine,
			std::sin(preferences.lightElevation),
			std::cos(preferences.lightAzimuth) * cosine);
	}

	ParticlePreviewPreferences loadParticlePreviewPreferences(std::filesystem::path const& path)
	{
		ParticlePreviewPreferences result;
		std::ifstream input(path);
		if (!input) return result;
		for (std::string line; std::getline(input, line);)
		{
			auto const separator = line.find('=');
			if (separator == std::string::npos) continue;
			auto const key = line.substr(0, separator);
			auto const value = line.substr(separator + 1);
			if (key == "graph") result.graph = value == "legacy" ? PreviewGraph::Legacy : PreviewGraph::Pbr;
			else if (key == "cameraTarget") readVec3(value, result.cameraTarget);
			else if (key == "cameraYaw") readFloat(value, result.cameraYaw);
			else if (key == "cameraPitch") readFloat(value, result.cameraPitch);
			else if (key == "cameraDistance") readFloat(value, result.cameraDistance);
			else if (key == "studioPreset")
			{
				float preset = 0.0f;
				if (readFloat(value, preset) && preset >= 0.0f) result.studioPreset = StudioPreset(uint32_t(preset));
			}
			else if (key == "floorGrid") readBool(value, result.floorGrid);
			else if (key == "lightEnabled") readBool(value, result.lightEnabled);
			else if (key == "lightAzimuth") readFloat(value, result.lightAzimuth);
			else if (key == "lightElevation") readFloat(value, result.lightElevation);
			else if (key == "lightDistance") readFloat(value, result.lightDistance);
			else if (key == "lightColour") readVec3(value, result.lightColour);
			else if (key == "lightIntensity") readFloat(value, result.lightIntensity);
			else if (key == "lightAutoOrbit") readBool(value, result.lightAutoOrbit);
			else if (key == "lightAutoOrbitSpeed") readFloat(value, result.lightAutoOrbitSpeed);
		}
		sanitize(result);
		return result;
	}

	void saveParticlePreviewPreferences(std::filesystem::path const& path,
		ParticlePreviewPreferences const& preferences)
	{
		auto value = preferences;
		sanitize(value);
		if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
		auto temporary = path;
		temporary += ".tmp";
		{
			std::ofstream output(temporary, std::ios::trunc);
			if (!output) throw std::runtime_error("Could not write Particle Editor preview preferences.");
			output << std::setprecision(9)
				<< "version=1\n"
				<< "graph=" << (value.graph == PreviewGraph::Pbr ? "pbr" : "legacy") << '\n'
				<< "cameraTarget=" << value.cameraTarget.x << ',' << value.cameraTarget.y << ',' << value.cameraTarget.z << '\n'
				<< "cameraYaw=" << value.cameraYaw << '\n'
				<< "cameraPitch=" << value.cameraPitch << '\n'
				<< "cameraDistance=" << value.cameraDistance << '\n'
				<< "studioPreset=" << uint32_t(value.studioPreset) << '\n'
				<< "floorGrid=" << (value.floorGrid ? "true" : "false") << '\n'
				<< "lightEnabled=" << (value.lightEnabled ? "true" : "false") << '\n'
				<< "lightAzimuth=" << value.lightAzimuth << '\n'
				<< "lightElevation=" << value.lightElevation << '\n'
				<< "lightDistance=" << value.lightDistance << '\n'
				<< "lightColour=" << value.lightColour.x << ',' << value.lightColour.y << ',' << value.lightColour.z << '\n'
				<< "lightIntensity=" << value.lightIntensity << '\n'
				<< "lightAutoOrbit=" << (value.lightAutoOrbit ? "true" : "false") << '\n'
				<< "lightAutoOrbitSpeed=" << value.lightAutoOrbitSpeed << '\n';
			if (!output) throw std::runtime_error("Could not finish writing Particle Editor preview preferences.");
		}
		std::error_code error;
		std::filesystem::rename(temporary, path, error);
		if (error)
		{
			std::filesystem::remove(path, error);
			error.clear();
			std::filesystem::rename(temporary, path, error);
		}
		if (error)
		{
			std::filesystem::remove(temporary);
			throw std::runtime_error("Could not replace Particle Editor preview preferences: " + error.message());
		}
	}

	bool runParticlePreviewPreferenceTests(std::string* failure)
	{
		auto fail = [&](std::string message) { if (failure) *failure = std::move(message); return false; };
		try
		{
			mpp::ParticleEffectBounds bounds{ { 2.0f, 3.0f, 4.0f }, { 4.0f, 6.0f, 8.0f } };
			auto bounded = studioVolumeForBounds(bounds);
			if (bounded.center != bounds.center || bounded.size != glm::vec3(6.0f, 9.0f, 12.0f))
				return fail("bounded particle effects do not size the studio to 1.5 times their bounds");
			auto fallback = studioVolumeForBounds(std::nullopt);
			if (fallback.size != glm::vec3(12.0f, 8.0f, 12.0f))
				return fail("unbounded particle effects do not receive the fixed studio");
			auto hidden = obstructingStudioFaces(bounded, bounded.center + bounded.size);
			if ((hidden & (StudioRight | StudioCeiling | StudioFront)) !=
				(StudioRight | StudioCeiling | StudioFront) || (hidden & StudioFloor) != 0u)
				return fail("camera-side studio faces are not dynamically identified");

			auto root = std::filesystem::temp_directory_path() / "mpp-particle-preview-preferences-tests";
			std::filesystem::remove_all(root);
			std::filesystem::create_directories(root);
			auto path = root / "preview.ini";
			auto particlePath = root / "asset.particle.yaml";
			auto particleEffect = ParticleDocument::makeStarterEffect();
			mpp::resource_parsers::ParticleEffectSerializer::toFile(particleEffect, particlePath.string());
			std::ifstream particleInput(particlePath, std::ios::binary);
			std::string particleYamlBefore((std::istreambuf_iterator<char>(particleInput)), {});
			particleInput.close();
			ParticlePreviewPreferences expected;
			expected.graph = PreviewGraph::Legacy;
			expected.cameraTarget = { 1.0f, 2.0f, 3.0f };
			expected.cameraYaw = 0.45f;
			expected.cameraPitch = -0.25f;
			expected.cameraDistance = 13.0f;
			expected.studioPreset = StudioPreset::Warm;
			expected.floorGrid = false;
			expected.lightEnabled = false;
			expected.lightAzimuth = 1.1f;
			expected.lightElevation = 0.4f;
			expected.lightDistance = 9.0f;
			expected.lightColour = { 0.2f, 0.4f, 0.8f };
			expected.lightIntensity = 7.0f;
			expected.lightAutoOrbit = true;
			expected.lightAutoOrbitSpeed = -0.7f;
			saveParticlePreviewPreferences(path, expected);
			auto actual = loadParticlePreviewPreferences(path);
			std::ifstream particleAfterInput(particlePath, std::ios::binary);
			std::string particleYamlAfter((std::istreambuf_iterator<char>(particleAfterInput)), {});
			std::ifstream preferenceInput(path, std::ios::binary);
			std::string preferenceText((std::istreambuf_iterator<char>(preferenceInput)), {});
			particleAfterInput.close();
			preferenceInput.close();
			std::filesystem::remove_all(root);
			if (particleYamlBefore.empty() || particleYamlAfter != particleYamlBefore ||
				particleYamlAfter.find("cameraYaw") != std::string::npos ||
				preferenceText.find("graph=legacy") == std::string::npos || preferenceText.find("lightIntensity=7") == std::string::npos)
				return fail("preview choices entered particle YAML instead of remaining editor preferences");
			if (actual.graph != expected.graph || actual.cameraTarget != expected.cameraTarget ||
				!nearlyEqual(actual.cameraYaw, expected.cameraYaw) || !nearlyEqual(actual.cameraPitch, expected.cameraPitch) ||
				!nearlyEqual(actual.cameraDistance, expected.cameraDistance) || actual.studioPreset != expected.studioPreset ||
				actual.floorGrid != expected.floorGrid || actual.lightEnabled != expected.lightEnabled ||
				!nearlyEqual(actual.lightAzimuth, expected.lightAzimuth) || !nearlyEqual(actual.lightElevation, expected.lightElevation) ||
				!nearlyEqual(actual.lightDistance, expected.lightDistance) || actual.lightColour != expected.lightColour ||
				!nearlyEqual(actual.lightIntensity, expected.lightIntensity) || actual.lightAutoOrbit != expected.lightAutoOrbit ||
				!nearlyEqual(actual.lightAutoOrbitSpeed, expected.lightAutoOrbitSpeed))
				return fail("editor-owned graph, camera, studio, material, and light preferences did not round-trip");
			if (failure) failure->clear();
			return true;
		}
		catch (std::exception const& error) { return fail(error.what()); }
	}
}
