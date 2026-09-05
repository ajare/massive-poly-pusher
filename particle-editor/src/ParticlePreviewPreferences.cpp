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

		template<size_t N>
		bool readArray(std::string text, std::array<float, N>& value)
		{
			std::replace(text.begin(), text.end(), ',', ' ');
			std::istringstream input(text);
			std::array<float, N> candidate{};
			for (auto& item : candidate)
				if (!(input >> item) || !std::isfinite(item)) return false;
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
			if (!std::all_of(value.signedDistanceFieldTransform.begin(), value.signedDistanceFieldTransform.end(),
				[](float component) { return std::isfinite(component); }))
				value.signedDistanceFieldTransform = ParticlePreviewPreferences{}.signedDistanceFieldTransform;
			value.signedDistanceFieldScale = std::isfinite(value.signedDistanceFieldScale) &&
				value.signedDistanceFieldScale > 0.0f ? value.signedDistanceFieldScale : 1.0f;
			value.signedDistanceFieldIsoValue = std::isfinite(value.signedDistanceFieldIsoValue) ?
				value.signedDistanceFieldIsoValue : 0.0f;
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

	std::vector<mpp::ParticleCollider> studioColliders(StudioVolume const& studio)
	{
		auto const minimum = studio.center - studio.size * 0.5f;
		auto const maximum = studio.center + studio.size * 0.5f;
		auto plane = [](glm::vec3 normal, float distance)
		{
			mpp::ParticleCollider collider;
			collider.shapeAndPadding[0] = uint32_t(mpp::ParticleColliderShape::Plane);
			collider.first = { normal.x, normal.y, normal.z, distance };
			return collider;
		};
		// No ceiling is registered: this is a floor plus four walls, and is
		// intentionally independent of camera-driven studio-face visibility.
		return {
			plane({ 0.0f, 1.0f, 0.0f }, minimum.y),
			plane({ 1.0f, 0.0f, 0.0f }, minimum.x),
			plane({ -1.0f, 0.0f, 0.0f }, -maximum.x),
			plane({ 0.0f, 0.0f, 1.0f }, minimum.z),
			plane({ 0.0f, 0.0f, -1.0f }, -maximum.z)
		};
	}

	std::vector<std::string> particlePreviewInputWarnings(
		mpp::ParticleEffectSpecification const& specification,
		ParticlePreviewPreferences const& preferences, ParticlePreviewInputStatus const& status)
	{
		bool vectorField = false;
		bool analytical = false;
		bool signedDistanceField = false;
		bool screenSpace = false;
		for (auto const& authored : specification.emitterTemplates)
		{
			auto const& simulation = authored.value.simulation;
			auto const modules = simulation.shapeSeedModulesBudget[2];
			vectorField |= (modules & uint32_t(mpp::ParticleBehaviourModule::VectorField)) != 0u;
			if ((modules & uint32_t(mpp::ParticleBehaviourModule::Collision)) == 0u) continue;
			auto const sources = simulation.collisionConfiguration[0];
			analytical |= (sources & uint32_t(mpp::ParticleCollisionSource::Analytical)) != 0u;
			signedDistanceField |= (sources & uint32_t(mpp::ParticleCollisionSource::SignedDistanceField)) != 0u;
			screenSpace |= (sources & uint32_t(mpp::ParticleCollisionSource::ScreenSpace)) != 0u;
		}

		std::vector<std::string> warnings;
		if (vectorField && !status.vectorFieldAvailable)
			warnings.push_back(preferences.vectorFieldResource.empty() ?
				"Vector field is enabled, but no preview vector-field resource is selected." :
				"Vector field is enabled, but preview resource '" + preferences.vectorFieldResource + "' is unavailable or is not a 3D texture.");
		if (analytical && !preferences.studioCollisions)
			warnings.push_back("Analytical collision is enabled, but preview studio collisions are disabled.");
		if (signedDistanceField && !status.signedDistanceFieldAvailable)
			warnings.push_back(preferences.signedDistanceFieldResource.empty() ?
				"Signed-distance-field collision is enabled, but no preview SDF resource is selected." :
				"Signed-distance-field collision is enabled, but preview resource '" + preferences.signedDistanceFieldResource + "' is unavailable, is not a 3D texture, or has invalid transform/scale/isovalue settings.");
		if (screenSpace && !status.screenSpaceDepthAvailable)
			warnings.push_back("Screen-space collision is awaiting retained depth from the active preview graph; it starts after that graph completes a frame.");
		return warnings;
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
			else if (key == "vectorFieldResource") result.vectorFieldResource = value;
			else if (key == "signedDistanceFieldResource") result.signedDistanceFieldResource = value;
			else if (key == "signedDistanceFieldTransform") readArray(value, result.signedDistanceFieldTransform);
			else if (key == "signedDistanceFieldScale") readFloat(value, result.signedDistanceFieldScale);
			else if (key == "signedDistanceFieldIsoValue") readFloat(value, result.signedDistanceFieldIsoValue);
			else if (key == "studioCollisions") readBool(value, result.studioCollisions);
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
				<< "lightAutoOrbitSpeed=" << value.lightAutoOrbitSpeed << '\n'
				<< "vectorFieldResource=" << value.vectorFieldResource << '\n'
				<< "signedDistanceFieldResource=" << value.signedDistanceFieldResource << '\n'
				<< "signedDistanceFieldTransform=";
			for (size_t index = 0; index < value.signedDistanceFieldTransform.size(); ++index)
			{
				if (index) output << ',';
				output << value.signedDistanceFieldTransform[index];
			}
			output << '\n'
				<< "signedDistanceFieldScale=" << value.signedDistanceFieldScale << '\n'
				<< "signedDistanceFieldIsoValue=" << value.signedDistanceFieldIsoValue << '\n'
				<< "studioCollisions=" << (value.studioCollisions ? "true" : "false") << '\n';
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
			auto collidersBeforeOrbit = studioColliders(bounded);
			(void)obstructingStudioFaces(bounded, bounded.center - bounded.size);
			auto collidersAfterOrbit = studioColliders(bounded);
			if (collidersBeforeOrbit.size() != 5u || collidersAfterOrbit.size() != collidersBeforeOrbit.size())
				return fail("studio collision did not register a stable floor and four walls");
			for (size_t index = 0; index < collidersBeforeOrbit.size(); ++index)
				if (collidersBeforeOrbit[index].first != collidersAfterOrbit[index].first)
					return fail("camera-driven visual face hiding changed studio colliders");

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
			expected.vectorFieldResource = "Fields/Wind";
			expected.signedDistanceFieldResource = "Fields/RoomSdf";
			expected.signedDistanceFieldTransform[12] = 0.25f;
			expected.signedDistanceFieldTransform[13] = -0.5f;
			expected.signedDistanceFieldScale = 2.5f;
			expected.signedDistanceFieldIsoValue = 0.4f;
			expected.studioCollisions = true;
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
				particleYamlAfter.find("Fields/Wind") != std::string::npos ||
				particleYamlAfter.find("studioCollisions") != std::string::npos ||
				preferenceText.find("graph=legacy") == std::string::npos ||
				preferenceText.find("vectorFieldResource=Fields/Wind") == std::string::npos ||
				preferenceText.find("studioCollisions=true") == std::string::npos)
				return fail("preview graph and collision inputs entered particle YAML instead of remaining editor preferences");
			if (actual.graph != expected.graph || actual.cameraTarget != expected.cameraTarget ||
				!nearlyEqual(actual.cameraYaw, expected.cameraYaw) || !nearlyEqual(actual.cameraPitch, expected.cameraPitch) ||
				!nearlyEqual(actual.cameraDistance, expected.cameraDistance) || actual.studioPreset != expected.studioPreset ||
				actual.floorGrid != expected.floorGrid || actual.lightEnabled != expected.lightEnabled ||
				!nearlyEqual(actual.lightAzimuth, expected.lightAzimuth) || !nearlyEqual(actual.lightElevation, expected.lightElevation) ||
				!nearlyEqual(actual.lightDistance, expected.lightDistance) || actual.lightColour != expected.lightColour ||
				!nearlyEqual(actual.lightIntensity, expected.lightIntensity) || actual.lightAutoOrbit != expected.lightAutoOrbit ||
				!nearlyEqual(actual.lightAutoOrbitSpeed, expected.lightAutoOrbitSpeed) ||
				actual.vectorFieldResource != expected.vectorFieldResource ||
				actual.signedDistanceFieldResource != expected.signedDistanceFieldResource ||
				actual.signedDistanceFieldTransform != expected.signedDistanceFieldTransform ||
				!nearlyEqual(actual.signedDistanceFieldScale, expected.signedDistanceFieldScale) ||
				!nearlyEqual(actual.signedDistanceFieldIsoValue, expected.signedDistanceFieldIsoValue) ||
				actual.studioCollisions != expected.studioCollisions)
				return fail("editor-owned graph, studio, light, and collision-input preferences did not round-trip");

			auto requiredInputs = ParticleDocument::makeStarterEffect();
			auto& inputSimulation = requiredInputs.emitterTemplates[0].value.simulation;
			inputSimulation.shapeSeedModulesBudget[2] = uint32_t(mpp::ParticleBehaviourModule::VectorField) |
				uint32_t(mpp::ParticleBehaviourModule::Collision);
			inputSimulation.collisionConfiguration[0] = uint32_t(mpp::ParticleCollisionSource::ScreenSpace) |
				uint32_t(mpp::ParticleCollisionSource::Analytical) |
				uint32_t(mpp::ParticleCollisionSource::SignedDistanceField);
			ParticlePreviewPreferences missing;
			auto warnings = particlePreviewInputWarnings(requiredInputs, missing, {});
			if (warnings.size() != 4u || !warnings[0].starts_with("Vector field") ||
				!warnings[1].starts_with("Analytical collision") ||
				!warnings[2].starts_with("Signed-distance-field collision") ||
				!warnings[3].starts_with("Screen-space collision"))
				return fail("enabled modules did not clearly diagnose every missing preview-global input");
			missing.vectorFieldResource = "Fields/Wind";
			missing.signedDistanceFieldResource = "Fields/RoomSdf";
			missing.studioCollisions = true;
			ParticlePreviewInputStatus available{ true, true, true };
			if (!particlePreviewInputWarnings(requiredInputs, missing, available).empty())
				return fail("available preview-global inputs retained missing-input warnings");
			inputSimulation.shapeSeedModulesBudget[2] = 0u;
			if (!particlePreviewInputWarnings(requiredInputs, ParticlePreviewPreferences{}, {}).empty())
				return fail("disabled behaviour modules still required preview-global inputs");
			if (failure) failure->clear();
			return true;
		}
		catch (std::exception const& error) { return fail(error.what()); }
	}
}
