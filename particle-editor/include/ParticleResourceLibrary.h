#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <mpp/Diagnostic.h>
#include <mpp/ParticleEffectSpecification.h>

namespace mpp
{
	class ResourceManager;
}

namespace particle_editor
{
	enum class ParticleResourceKind
	{
		Texture,
		Model,
		Material
	};

	struct ParticleResourceEntry
	{
		std::string name;
		ParticleResourceKind kind{ ParticleResourceKind::Texture };
	};

	// Editor-only catalog and runtime declaration of resources used by particle
	// effect previews. Particle assets retain only the logical names exposed here.
	class ParticleResourceLibrary
	{
		mpp::ResourceManager* mResources{};
		std::filesystem::path mRoot;
		std::filesystem::path mLibraryPath;
		std::string mLibraryName;
		std::vector<ParticleResourceEntry> mEntries;
		std::vector<std::string> mOwnedResourceNames;
		mpp::DiagnosticBag mDiagnostics;

		std::optional<ParticleResourceKind> resolvedKind(std::string const& name) const;

	public:
		explicit ParticleResourceLibrary(mpp::ResourceManager* resources = nullptr);
		~ParticleResourceLibrary();
		ParticleResourceLibrary(ParticleResourceLibrary const&) = delete;
		ParticleResourceLibrary& operator=(ParticleResourceLibrary const&) = delete;

		bool reload(std::filesystem::path root, std::filesystem::path library);
		void clear() noexcept;
		std::vector<std::string> names(ParticleResourceKind kind) const;
		bool resolves(std::string const& name, ParticleResourceKind kind) const;
		mpp::DiagnosticBag referenceDiagnostics(mpp::ParticleEffectSpecification const& specification,
			std::string const& sourceName = {}) const;

		std::filesystem::path const& root() const { return mRoot; }
		std::filesystem::path const& libraryPath() const { return mLibraryPath; }
		std::string const& libraryName() const { return mLibraryName; }
		std::vector<ParticleResourceEntry> const& entries() const { return mEntries; }
		mpp::DiagnosticBag const& diagnostics() const { return mDiagnostics; }
	};

	bool runParticleResourceLibraryTests(std::string* failure = nullptr);
}
