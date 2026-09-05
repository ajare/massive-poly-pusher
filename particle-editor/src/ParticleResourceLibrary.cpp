#include "ParticleResourceLibrary.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <set>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include <mpp/MppModelStream.h>
#include <mpp/ParticleEffectBounds.h>
#include <mpp/Resource.h>
#include <mpp/ResourceManager.h>
#include <mpp/ResourceStream.h>
#include <mpp/data/StructuredData.h>
#include <mpp/resource-parsers/FileBasicMaterialStream.h>
#include <mpp/resource-parsers/FilePbrMaterialStream.h>
#include <mpp/resource-parsers/FileParticleEffectStream.h>
#include <mpp/resource-parsers/FileStream.h>
#include <mpp/resource-parsers/FileTextureStream.h>
#include <mpp/resource-parsers/ParticleEffectParser.h>
#include <mpp/resource-parsers/ParticleEffectSerializer.h>

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
			case ParticleResourceKind::ParticleEffect: return "ParticleEffect";
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
			if (type == "ParticleEffect") return ParticleResourceKind::ParticleEffect;
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
				else if (item.first == "ParticleEffect") kind = ParticleResourceKind::ParticleEffect;
				else
				{
					libraryError("Unsupported particle preview resource type '" + item.first +
						"'; expected Texture, Model, BasicMaterial, PbrMaterial, or ParticleEffect.");
					continue;
				}

				try
				{
					auto localName = scalar(item.second, "name", "ResourceLibrary/Resources/" + item.first);
					auto logicalName = mLibraryName + "::" + localName;
					if (!logicalNames.emplace(logicalName).second)
						throw std::runtime_error("Duplicate logical resource name '" + logicalName + "'.");

					mpp::ResourceStreamPtr stream;
					std::filesystem::path sourcePath;
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
						rejectUnknown(item.second, { "name", "filename" }, "ResourceLibrary/Resources/" + item.first);
						sourcePath = std::filesystem::path(scalar(item.second, "filename",
							"ResourceLibrary/Resources/" + item.first));
						if (sourcePath.is_relative()) sourcePath = mLibraryPath.parent_path() / sourcePath;
						sourcePath = std::filesystem::absolute(sourcePath).lexically_normal();
						if (!std::filesystem::is_regular_file(sourcePath))
							throw std::runtime_error(item.first + " resource '" + logicalName +
								"' refers to missing file '" + sourcePath.string() + "'.");
						if (item.first == "ParticleEffect")
							stream = std::make_shared<mpp::resource_parsers::FileParticleEffectStream>(mResources, sourcePath.string());
						else stream = std::make_shared<mpp::MppModelStream>(mResources, sourcePath.string());
					}

					if (mResources)
					{
						auto resource = mResources->declareResource(logicalName, stream).first;
						if (resourceKind(resource->getType()) != kind)
							throw std::runtime_error("Resource '" + logicalName + "' was declared with the wrong MPP type.");
						mOwnedResourceNames.push_back(logicalName);
					}
					mEntries.push_back({ std::move(logicalName), kind, std::move(sourcePath) });
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

	std::optional<std::filesystem::path> ParticleResourceLibrary::particleEffectPath(std::string const& name) const
	{
		auto found = std::find_if(mEntries.begin(), mEntries.end(), [&](auto const& entry)
			{ return entry.name == name && entry.kind == ParticleResourceKind::ParticleEffect; });
		if (found == mEntries.end() || found->sourcePath.empty()) return std::nullopt;
		return found->sourcePath;
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

		std::unordered_set<std::string> visiting;
		if (!sourceName.empty()) visiting.emplace(std::filesystem::absolute(sourceName).lexically_normal().string());
		std::function<void(mpp::ParticleEffectSpecification const&, std::string const&)> visitChildren;
		visitChildren = [&](mpp::ParticleEffectSpecification const& parent, std::string const& parentSource)
		{
			for (size_t index = 0; index < parent.childEffects.size(); ++index)
			{
				auto const& child = parent.childEffects[index];
				auto path = "/ParticleEffect/ChildEffects/ChildEffect[" + std::to_string(index) + "]/effect";
				auto actual = resolvedKind(child.effect);
				if (!actual)
				{
					result.error("MPP-PARTICLE-EDITOR-CHILD-001", "Child particle effect '" + child.effect +
						"' is unresolved; choose a ParticleEffect from the configured resource library.", { parentSource, path });
					continue;
				}
				if (*actual != ParticleResourceKind::ParticleEffect)
				{
					result.error("MPP-PARTICLE-EDITOR-CHILD-002", "Child resource '" + child.effect +
						"' has type " + kindName(*actual) + ", but ParticleEffect is required.", { parentSource, path });
					continue;
				}
				auto childPath = particleEffectPath(child.effect);
				if (!childPath) continue;
				auto identity = std::filesystem::absolute(*childPath).lexically_normal().string();
				if (!visiting.emplace(identity).second)
				{
					result.error("MPP-PARTICLE-EDITOR-CHILD-003", "Child particle effect cycle resolved through '" +
						child.effect + "'.", { parentSource, path });
					continue;
				}
				auto parsed = mpp::resource_parsers::ParticleEffectParser::fromFile(childPath->string());
				if (!parsed.succeeded()) result.append(parsed.diagnostics);
				else visitChildren(parsed.specification, childPath->string());
				visiting.erase(identity);
			}
		};
		visitChildren(specification, sourceName);
		return result;
	}

	ParticleAggregateBoundsStatus ParticleResourceLibrary::aggregateBounds(
		mpp::ParticleEffectSpecification const& specification, std::string const& sourceName) const
	{
		ParticleAggregateBoundsStatus status{ specification.bounds, true, {} };
		if (!status.bounds) status.reason = "This particle effect has no authored bounds.";
		std::unordered_set<std::string> visiting;
		if (!sourceName.empty()) visiting.emplace(std::filesystem::absolute(sourceName).lexically_normal().string());
		std::function<std::optional<mpp::ParticleEffectBounds>(mpp::ParticleEffectSpecification const&)> aggregate;
		aggregate = [&](mpp::ParticleEffectSpecification const& parent) -> std::optional<mpp::ParticleEffectBounds>
		{
			if (!parent.bounds) return std::nullopt;
			auto bounds = parent.bounds;
			for (auto const& child : parent.childEffects)
			{
				auto childPath = particleEffectPath(child.effect);
				if (!childPath)
				{
					status.complete = false;
					status.reason = "Aggregate bounds are unavailable because child '" + child.effect + "' is unresolved or has the wrong type.";
					return std::nullopt;
				}
				auto identity = std::filesystem::absolute(*childPath).lexically_normal().string();
				if (!visiting.emplace(identity).second)
				{
					status.complete = false; status.reason = "Aggregate bounds are unavailable because the resolved child graph contains a cycle.";
					return std::nullopt;
				}
				auto parsed = mpp::resource_parsers::ParticleEffectParser::fromFile(childPath->string());
				if (!parsed.succeeded())
				{
					visiting.erase(identity); status.complete = false;
					status.reason = "Aggregate bounds are unavailable because child '" + child.effect + "' is invalid.";
					return std::nullopt;
				}
				auto childBounds = aggregate(parsed.specification);
				visiting.erase(identity);
				if (!childBounds)
				{
					if (status.reason.empty()) status.reason = "Unbounded: child '" + child.effect + "' has an unbounded participating branch.";
					return std::nullopt;
				}
				bounds = mpp::combineParticleEffectBounds(*bounds,
					mpp::transformParticleEffectBounds(*childBounds, child.transform));
			}
			return bounds;
		};
		status.bounds = aggregate(specification);
		if (!status.bounds && status.reason.empty()) status.reason = "Unbounded: a participating branch has no authored bounds.";
		return status;
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
			auto particle = [](std::string name)
			{
				mpp::ParticleEffectSpecification effect; effect.version = 2u; effect.name = std::move(name);
				effect.bounds = mpp::ParticleEffectBounds{ { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f } };
				mpp::ParticleEffectSpecification::EmitterTemplate emitter; emitter.name = "Emitter";
				emitter.value.simulation.shapeSeedModulesBudget[3] = 1u; effect.maximumParticleCount = 1u;
				effect.emitterTemplates.push_back(std::move(emitter)); return effect;
			};
			auto smoke = particle("Smoke");
			mpp::resource_parsers::ParticleEffectSerializer::toFile(smoke, (root / "smoke.particle.yaml").string());
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
				"        baseColourFactor: 1 1 1 1\n"
				"    ParticleEffect:\n"
				"      name: Smoke\n"
				"      filename: smoke.particle.yaml\n";
			library.close();

			ParticleResourceLibrary catalog;
			if (!catalog.reload(root, "library.yaml") || catalog.entries().size() != 4u ||
				!catalog.resolves("TestLibrary::Atlas", ParticleResourceKind::Texture) ||
				!catalog.resolves("TestLibrary::Debris", ParticleResourceKind::Model) ||
				!catalog.resolves("TestLibrary::Override", ParticleResourceKind::Material) ||
				!catalog.resolves("TestLibrary::Smoke", ParticleResourceKind::ParticleEffect) ||
				catalog.particleEffectPath("TestLibrary::Smoke") != root / "smoke.particle.yaml")
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

			mpp::ParticleEffectSpecification composed = particle("Composed");
			mpp::ParticleEffectSpecification::ChildEffect boundedChild; boundedChild.effect = "TestLibrary::Smoke";
			boundedChild.transform[3][0] = 3.0f; composed.childEffects.push_back(boundedChild);
			auto aggregate = catalog.aggregateBounds(composed);
			if (!aggregate.complete || !aggregate.bounds || aggregate.bounds->center.x != 1.5f || aggregate.bounds->size.x != 5.0f)
				return fail("recursive transformed child particle effect bounds were not presented conservatively");
			composed.childEffects.push_back({ "Missing::Child" });
			if (catalog.aggregateBounds(composed).complete || catalog.aggregateBounds(composed).bounds)
				return fail("an unresolved child branch did not make aggregate-bound status unavailable");
			smoke.childEffects.push_back({ "TestLibrary::Smoke" });
			mpp::resource_parsers::ParticleEffectSerializer::toFile(smoke, (root / "smoke.particle.yaml").string());
			composed.childEffects.clear();
			composed.childEffects.push_back({ "Missing::Child" });
			composed.childEffects.push_back({ "TestLibrary::Atlas" });
			composed.childEffects.push_back({ "TestLibrary::Smoke" });
			auto childDiagnostics = catalog.referenceDiagnostics(composed, (root / "parent.particle.yaml").string());
			if (childDiagnostics.count(mpp::DiagnosticSeverity::Error) < 3u)
				return fail("missing, wrong-type, and cyclic resolved child references did not produce diagnostics");
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
