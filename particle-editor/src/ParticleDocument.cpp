#include "ParticleDocument.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

#include <mpp/ParticleEffectValidator.h>
#include <mpp/app/DocumentFile.h>
#include <mpp/resource-parsers/ParticleEffectParser.h>
#include <mpp/resource-parsers/ParticleEffectSerializer.h>

namespace particle_editor
{
	namespace
	{
		std::string readText(std::filesystem::path const& path)
		{
			std::ifstream input(path, std::ios::binary);
			if (!input) throw std::runtime_error("Could not read '" + path.string() + "'.");
			return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
		}

		std::filesystem::path canonicalParticleEffectPath(std::filesystem::path path)
		{
			auto filename = path.filename().string();
			std::transform(filename.begin(), filename.end(), filename.begin(), [](unsigned char value)
				{ return static_cast<char>(std::tolower(value)); });
			if (filename.ends_with(".particle.yaml")) return path;
			if (filename.ends_with(".particle.yml"))
			{
				path.replace_extension(".yaml");
				return path;
			}
			auto extension = path.extension().string();
			std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value)
				{ return static_cast<char>(std::tolower(value)); });
			if (extension == ".yaml" || extension == ".yml") path.replace_extension();
			path += ".particle.yaml";
			return path;
		}
	}

	ParticleDocument::ParticleDocument()
	{
		createNew();
	}

	mpp::ParticleEffectSpecification ParticleDocument::makeStarterEffect()
	{
		using namespace mpp;
		ParticleEffectSpecification effect;
		effect.version = 2u;
		effect.name = "Untitled Particle Effect";
		effect.maximumParticleCount = 1024u;
		effect.bounds = ParticleEffectBounds{ { 0.0f, 1.5f, 0.0f }, { 4.0f, 4.0f, 4.0f } };

		ParticleEffectSpecification::EmitterTemplate authored;
		authored.name = "Emitter";
		auto& emitter = authored.value;
		auto& simulation = emitter.simulation;
		simulation.shapeSeedModulesBudget = {
			uint32_t(ParticleSpawnShape::Point), 17u, 0u, 1024u
		};
		simulation.emissionState = { 0u, 1u, 0u, 0u };
		simulation.emissionRateAndPadding[0] = 20.0f;
		simulation.initialVelocityMin = { -0.15f, 0.6f, -0.15f, 0.0f };
		simulation.initialVelocityMax = { 0.15f, 1.2f, 0.15f, 0.0f };
		simulation.colourMin = { 1.0f, 0.2f, 0.05f, 0.65f };
		simulation.colourMax = { 1.0f, 0.85f, 0.25f, 1.0f };
		simulation.lifetimeSizeRanges = { 1.0f, 2.0f, 0.12f, 0.22f };

		auto& appearance = emitter.appearance;
		appearance.modes[2] = uint32_t(ParticleBillboardMode::CameraFacing);
		appearance.modes[3] = uint32_t(ParticleBlendClass::Additive);
		appearance.appearance[0] = 1.5f;

		emitter.curves[size_t(ParticleScalarCurve::Size)].keys = {
			{ 0.0f, 0.25f }, { 0.15f, 1.0f }, { 1.0f, 0.0f }
		};
		emitter.curves[size_t(ParticleScalarCurve::Alpha)].keys = {
			{ 0.0f, 0.0f }, { 0.1f, 1.0f }, { 0.8f, 0.8f }, { 1.0f, 0.0f }
		};
		effect.emitterTemplates.push_back(std::move(authored));
		return effect;
	}

	void ParticleDocument::refreshDiagnostics()
	{
		mDiagnostics = mpp::ParticleEffectValidator::validate(mSpecification, mPath.string());
	}

	void ParticleDocument::createNew()
	{
		mSpecification = makeStarterEffect();
		mPath.clear();
		mDirty = true;
		refreshDiagnostics();
	}

	bool ParticleDocument::open(std::filesystem::path const& path)
	{
		auto normalised = mpp::app::normaliseDocumentPath(path);
		auto parsed = mpp::resource_parsers::ParticleEffectParser::fromFile(normalised.string());
		mDiagnostics = parsed.diagnostics;
		if (!parsed.succeeded()) return false;

		bool const upgraded = parsed.specification.version != 2u;
		mSpecification = std::move(parsed.specification);
		mSpecification.version = 2u;
		mPath = std::move(normalised);
		mDirty = upgraded;
		refreshDiagnostics();
		return !mDiagnostics.hasErrors();
	}

	void ParticleDocument::save(std::filesystem::path const& path)
	{
		if (path.empty()) throw std::invalid_argument("Particle effect save path is empty.");
		mSpecification.version = 2u;
		refreshDiagnostics();
		auto normalised = mpp::app::normaliseDocumentPath(canonicalParticleEffectPath(path));
		mpp::resource_parsers::ParticleEffectSerializer::toFile(mSpecification, normalised.string());
		mPath = std::move(normalised);
		mDirty = false;
		refreshDiagnostics();
	}

	void ParticleDocument::save()
	{
		if (mPath.empty()) throw std::logic_error("The particle effect does not have a save path.");
		save(mPath);
	}

	std::string ParticleDocument::displayName() const
	{
		return mPath.empty() ? "Untitled Particle Effect" : mPath.filename().string();
	}

	bool runParticleDocumentTests(std::string* failure)
	{
		auto fail = [&](std::string message)
		{
			if (failure) *failure = std::move(message);
			return false;
		};
		try
		{
			auto first = ParticleDocument::makeStarterEffect();
			auto second = ParticleDocument::makeStarterEffect();
			if (first.version != 2u || first.name != "Untitled Particle Effect" || first.maximumParticleCount != 1024u ||
				!first.bounds || first.bounds->center != glm::vec3(0.0f, 1.5f, 0.0f) ||
				first.bounds->size != glm::vec3(4.0f) || first.emitterTemplates.size() != 1u)
				return fail("the version-2 starter particle effect contract changed");
			auto const& emitter = first.emitterTemplates.front().value;
			auto const& repeatedEmitter = second.emitterTemplates.front().value;
			if (first.emitterTemplates.front().name != "Emitter" ||
				emitter.simulation.shapeSeedModulesBudget != std::array<uint32_t, 4>{ 0u, 17u, 0u, 1024u } ||
				emitter.simulation.emissionRateAndPadding[0] != 20.0f ||
				emitter.simulation.initialVelocityMin != std::array<float, 4>{ -0.15f, 0.6f, -0.15f, 0.0f } ||
				emitter.simulation.initialVelocityMax != std::array<float, 4>{ 0.15f, 1.2f, 0.15f, 0.0f } ||
				emitter.simulation.lifetimeSizeRanges != std::array<float, 4>{ 1.0f, 2.0f, 0.12f, 0.22f } ||
				emitter.appearance.modes[2] != uint32_t(mpp::ParticleBillboardMode::CameraFacing) ||
				emitter.appearance.modes[3] != uint32_t(mpp::ParticleBlendClass::Additive) ||
				emitter.curves[size_t(mpp::ParticleScalarCurve::Size)].keys.size() != 3u ||
				emitter.curves[size_t(mpp::ParticleScalarCurve::Alpha)].keys.size() != 4u ||
				repeatedEmitter.simulation.shapeSeedModulesBudget != emitter.simulation.shapeSeedModulesBudget ||
				repeatedEmitter.simulation.colourMin != emitter.simulation.colourMin ||
				repeatedEmitter.simulation.colourMax != emitter.simulation.colourMax)
				return fail("the deterministic starter emitter contract changed");
			if (mpp::ParticleEffectValidator::validate(first).hasErrors() ||
				mpp::ParticleEffectValidator::validate(second).hasErrors())
				return fail("the starter particle effect is not production-valid");

			auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
			auto selectedPath = std::filesystem::temp_directory_path() /
				("mpp-particle-editor-" + std::to_string(nonce) + ".yaml");
			auto path = selectedPath;
			path.replace_extension();
			path += ".particle.yaml";
			ParticleDocument document;
			document.save(selectedPath);
			if (document.path() != mpp::app::normaliseDocumentPath(path))
				return fail("the editor document did not enforce the canonical .particle.yaml suffix");
			auto canonical = readText(path);
			if (canonical.find("version: 2") == std::string::npos ||
				canonical.find("Bounds:") == std::string::npos || canonical.find("seed: 17") == std::string::npos)
				return fail("the production serializer omitted required starter fields");
			document.save();
			if (readText(path) != canonical)
				return fail("saving the same particle effect did not produce canonical YAML");
			ParticleDocument restored;
			if (!restored.open(path) || restored.specification().version != 2u ||
				restored.specification().emitterTemplates.front().value.simulation.shapeSeedModulesBudget[1] != 17u)
				return fail("the editor document did not open its production-serialized particle effect");
			auto restoredPath = restored.path();
			if (restored.open(path.string() + ".missing") || restored.path() != restoredPath ||
				restored.specification().name != "Untitled Particle Effect" || !restored.diagnostics().hasErrors())
				return fail("a failed production parse replaced the active editor document");
			std::error_code ignored;
			std::filesystem::remove(path, ignored);
			return true;
		}
		catch (std::exception const& error)
		{
			return fail(error.what());
		}
	}
}
