#include "ParticleResourceLibrary.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <set>
#include <stdexcept>
#include <utility>

#include <mpp/MppModelStream.h>
#include <mpp/Resource.h>
#include <mpp/ResourceManager.h>
#include <mpp/ResourceStream.h>
#include <mpp/data/StructuredData.h>
#include <mpp/resource-parsers/FileBasicMaterialStream.h>
#include <mpp/resource-parsers/FilePbrMaterialStream.h>
#include <mpp/resource-parsers/FileStream.h>
#include <mpp/resource-parsers/FileTextureStream.h>

namespace particle_editor
{
	namespace
	{
		using Data = mpp::data::StructuredData;

		char const* kindName(ParticleResourceKind kind)
		{
			switch (kind)
			{
			case ParticleResourceKind::Texture: return "Texture";
			case ParticleResourceKind::Model: return "Model";
			case ParticleResourceKind::Material: return "Material";
			}
			return "Resource";
		}

		void rejectUnknown(Data const& node, std::initializer_list<char const*> accepted,
			std::string const& context)
		{
			std::set<std::string> names;
			for (auto name : accepted) names.emplace(name);
			for (auto const& entry : node)
				if (!names.contains(entry.first))
					throw std::runtime_error("Unknown field '" + entry.first + "' in " + context + ".");
		}

		std::string scalar(Data const& node, char const* field, std::string const& context)
		{
			if (!node.hasEntry(field) || !node.getEntry(field).isValue() || node.getEntry(field).getValue().empty())
				throw std::runtime_error(context + " requires a non-empty '" + field + "' value.");
			return node.getEntry(field).getValue();
		}

		std::optional<ParticleResourceKind> resourceKind(std::string const& type)
		{
			if (type == "Texture") return ParticleResourceKind::Texture;
			if (type == "Model") return ParticleResourceKind::Model;
			if (type == "BasicMaterial" || type == "PbrMaterial") return ParticleResourceKind::Material;
			return std::nullopt;
		}
	}

	ParticleResourceLibrary::ParticleResourceLibrary(mpp::ResourceManager* resources)
		: mResources(resources)
	{
	}

	ParticleResourceLibrary::~ParticleResourceLibrary()
	{
		clear();
	}

	void ParticleResourceLibrary::clear() noexcept
	{
		if (mResources)
		{
			for (auto name = mOwnedResourceNames.rbegin(); name != mOwnedResourceNames.rend(); ++name)
			{
				try
				{
					if (mResources->getResource(*name, true)) mResources->deleteResourceTree(*name);
				}
				catch (...) {}
			}
		}
		mOwnedResourceNames.clear();
		mEntries.clear();
		mLibraryName.clear();
		mLibraryPath.clear();
		mRoot.clear();
		mDiagnostics.clear();
	}

	bool ParticleResourceLibrary::reload(std::filesystem::path root, std::filesystem::path library)
	{
		clear();
		mRoot = std::move(root);
		if (mRoot.empty()) mRoot = std::filesystem::current_path();
		if (mRoot.is_relative()) mRoot = std::filesystem::absolute(mRoot);
		mRoot = mRoot.lexically_normal();
		mLibraryPath = library.is_absolute() ? std::move(library) : mRoot / library;
		mLibraryPath = std::filesystem::absolute(mLibraryPath).lexically_normal();

		auto libraryError = [&](std::string message)
		{
			mDiagnostics.error("MPP-PARTICLE-EDITOR-RESOURCE-001", std::move(message),
				{ mLibraryPath.string(), "/ResourceLibrary" });
		};

		try
		{
			if (!std::filesystem::is_directory(mRoot))
				throw std::runtime_error("Configured particle resource root does not exist: " + mRoot.string());
			if (!std::filesystem::is_regular_file(mLibraryPath))
				throw std::runtime_error("Configured particle resource library does not exist: " + mLibraryPath.string());
			mpp::resource_parsers::FileStream libraryFile(mLibraryPath.string());
			auto const& rootNode = libraryFile.getStructuredData();
			if (rootNode.getName() != "ResourceLibrary")
				throw std::runtime_error("Particle resource library root must be ResourceLibrary.");
			rejectUnknown(rootNode, { "version", "name", "Resources" }, "ResourceLibrary");
			if (scalar(rootNode, "version", "ResourceLibrary") != "1")
				throw std::runtime_error("Particle resource library version must be 1.");
			mLibraryName = scalar(rootNode, "name", "ResourceLibrary");
			if (!rootNode.hasEntry("Resources")) return true;

			std::set<std::string> logicalNames;
			for (auto const& item : rootNode.getEntry("Resources"))
			{
				ParticleResourceKind kind;
				if (item.first == "Texture") kind = ParticleResourceKind::Texture;
				else if (item.first == "Model") kind = ParticleResourceKind::Model;
				else if (item.first == "BasicMaterial" || item.first == "PbrMaterial")
					kind = ParticleResourceKind::Material;
				else
				{
					libraryError("Unsupported particle preview resource type '" + item.first +
						"'; expected Texture, Model, BasicMaterial, or PbrMaterial.");
					continue;
				}

				try
				{
					auto localName = scalar(item.second, "name", "ResourceLibrary/Resources/" + item.first);
					auto logicalName = mLibraryName + "::" + localName;
					if (!logicalNames.emplace(logicalName).second)
						throw std::runtime_error("Duplicate logical resource name '" + logicalName + "'.");

					mpp::ResourceStreamPtr stream;
					if (item.first == "Texture")
						stream = std::make_shared<mpp::resource_parsers::FileTextureStream>(
							mResources, mLibraryPath.string(), item.second);
					else if (item.first == "BasicMaterial")
						stream = std::make_shared<mpp::resource_parsers::FileBasicMaterialStream>(
							mResources, mLibraryPath.string(), item.second);
					else if (item.first == "PbrMaterial")
						stream = std::make_shared<mpp::resource_parsers::FilePbrMaterialStream>(
							mResources, mLibraryPath.string(), item.second);
					else
					{
						rejectUnknown(item.second, { "name", "filename" }, "ResourceLibrary/Resources/Model");
						auto modelPath = std::filesystem::path(scalar(item.second, "filename",
							"ResourceLibrary/Resources/Model"));
						if (modelPath.is_relative()) modelPath = mLibraryPath.parent_path() / modelPath;
						modelPath = std::filesystem::absolute(modelPath).lexically_normal();
						if (!std::filesystem::is_regular_file(modelPath))
							throw std::runtime_error("Model resource '" + logicalName +
								"' refers to missing file '" + modelPath.string() + "'.");
						stream = std::make_shared<mpp::MppModelStream>(mResources, modelPath.string());
					}

					if (mResources)
					{
						auto resource = mResources->declareResource(logicalName, stream).first;
						if (resourceKind(resource->getType()) != kind)
							throw std::runtime_error("Resource '" + logicalName + "' was declared with the wrong MPP type.");
						mOwnedResourceNames.push_back(logicalName);
					}
					mEntries.push_back({ std::move(logicalName), kind });
				}
				catch (std::exception const& error)
				{
					mDiagnostics.error("MPP-PARTICLE-EDITOR-RESOURCE-002",
						"Could not declare " + item.first + " from the particle resource library: " + error.what(),
						{ mLibraryPath.string(), "/ResourceLibrary/Resources/" + item.first });
				}
			}
		}
		catch (std::exception const& error)
		{
			libraryError(error.what());
		}
		return !mDiagnostics.hasErrors();
	}

	std::optional<ParticleResourceKind> ParticleResourceLibrary::resolvedKind(std::string const& name) const
	{
		if (name.empty()) return std::nullopt;
		if (mResources)
			if (auto resource = mResources->getResource(name, true)) return resourceKind(resource->getType());
		auto found = std::find_if(mEntries.begin(), mEntries.end(), [&](auto const& entry) { return entry.name == name; });
		return found == mEntries.end() ? std::nullopt : std::optional(found->kind);
	}

	std::vector<std::string> ParticleResourceLibrary::names(ParticleResourceKind kind) const
	{
		std::vector<std::string> result;
		for (auto const& entry : mEntries)
			if (entry.kind == kind) result.push_back(entry.name);
		std::sort(result.begin(), result.end());
		return result;
	}

	bool ParticleResourceLibrary::resolves(std::string const& name, ParticleResourceKind kind) const
	{
		return resolvedKind(name) == kind;
	}

	mpp::DiagnosticBag ParticleResourceLibrary::referenceDiagnostics(
		mpp::ParticleEffectSpecification const& specification, std::string const& sourceName) const
	{
		mpp::DiagnosticBag result;
		auto check = [&](std::string const& name, ParticleResourceKind expected, std::string path)
		{
			if (name.empty()) return;
			auto actual = resolvedKind(name);
			if (!actual)
			{
				result.warning("MPP-PARTICLE-EDITOR-RESOURCE-003", "Logical " + std::string(kindName(expected)) +
					" resource '" + name + "' is unresolved. Choose a declared name from the editor resource library " +
					"or configure [Editor] resourceRoot/resourceLibrary; the authored name has been preserved.",
					{ sourceName, std::move(path) });
			}
			else if (*actual != expected)
			{
				result.warning("MPP-PARTICLE-EDITOR-RESOURCE-004", "Logical resource '" + name + "' resolves to " +
					kindName(*actual) + ", but this field requires " + kindName(expected) +
					"; the authored name has been preserved.", { sourceName, std::move(path) });
			}
		};

		for (size_t index = 0; index < specification.emitterTemplates.size(); ++index)
		{
			auto const& emitter = specification.emitterTemplates[index];
			auto base = "/ParticleEffect/Emitters/Emitter[" + std::to_string(index) + "]";
			check(emitter.albedoTexture, ParticleResourceKind::Texture, base + "/Appearance/texture");
			check(emitter.meshModel, ParticleResourceKind::Model, base + "/Mesh/model");
			check(emitter.meshMaterial, ParticleResourceKind::Material, base + "/Mesh/material");
		}
		return result;
	}

	bool runParticleResourceLibraryTests(std::string* failure)
	{
		auto fail = [failure](std::string message)
		{
			if (failure) *failure = std::move(message);
			return false;
		};
		auto root = std::filesystem::temp_directory_path() / "mpp-particle-resource-library-tests";
		std::error_code ignored;
		std::filesystem::remove_all(root, ignored);
		try
		{
			std::filesystem::create_directories(root);
			std::ofstream(root / "dummy.mppmodel", std::ios::binary) << "test";
			std::ofstream library(root / "library.yaml");
			library << "ResourceLibrary:\n"
				"  version: 1\n"
				"  name: TestLibrary\n"
				"  Resources:\n"
				"    Texture:\n"
				"      name: Atlas\n"
				"      target: 2D\n"
				"      filename: atlas.png\n"
				"    Model:\n"
				"      name: Debris\n"
				"      filename: dummy.mppmodel\n"
				"    PbrMaterial:\n"
				"      name: Override\n"
				"      MeshSpecification:\n"
				"        primitive: triangles\n"
				"        indexed: true\n"
				"        storage: static\n"
				"        Buffer:\n"
				"          - data: position3\n"
				"            type: float32\n"
				"      Surface:\n"
				"        baseColourFactor: 1 1 1 1\n";
			library.close();

			ParticleResourceLibrary catalog;
			if (!catalog.reload(root, "library.yaml") || catalog.entries().size() != 3u ||
				!catalog.resolves("TestLibrary::Atlas", ParticleResourceKind::Texture) ||
				!catalog.resolves("TestLibrary::Debris", ParticleResourceKind::Model) ||
				!catalog.resolves("TestLibrary::Override", ParticleResourceKind::Material))
			{
				std::string detail;
				for (auto const& diagnostic : catalog.diagnostics().getDiagnostics()) detail += " " + diagnostic.message;
				return fail("resource library did not expose qualified texture, model, and material names:" + detail);
			}

			mpp::ParticleEffectSpecification specification;
			mpp::ParticleEffectSpecification::EmitterTemplate emitter;
			emitter.albedoTexture = "Missing::Texture";
			emitter.meshModel = "TestLibrary::Atlas";
			emitter.meshMaterial = "Missing::Material";
			specification.emitterTemplates.push_back(emitter);
			auto diagnostics = catalog.referenceDiagnostics(specification, "effect.particle.yaml");
			if (diagnostics.count(mpp::DiagnosticSeverity::Warning) != 3u || diagnostics.hasErrors())
				return fail("unresolved and wrong-type particle resource references were not actionable warnings");
			auto const& values = diagnostics.getDiagnostics();
			if (values[0].location.elementPath.find("/Appearance/texture") == std::string::npos ||
				values[1].location.elementPath.find("/Mesh/model") == std::string::npos ||
				values[2].message.find("preserved") == std::string::npos)
				return fail("resource reference diagnostics did not identify the editable fields and preservation behavior");
		}
		catch (std::exception const& error)
		{
			std::filesystem::remove_all(root, ignored);
			return fail("resource library test threw: " + std::string(error.what()));
		}
		std::filesystem::remove_all(root, ignored);
		return true;
	}
}
