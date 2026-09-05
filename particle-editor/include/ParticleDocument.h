#pragma once

#include <filesystem>

#include <mpp/Diagnostic.h>
#include <mpp/ParticleEffectSpecification.h>

namespace particle_editor
{
	class ParticleDocument
	{
		mpp::ParticleEffectSpecification mSpecification;
		mpp::DiagnosticBag mDiagnostics;
		std::filesystem::path mPath;
		bool mDirty{ false };

		void refreshDiagnostics();

	public:
		ParticleDocument();

		static mpp::ParticleEffectSpecification makeStarterEffect();
		void createNew();
		bool open(std::filesystem::path const& path);
		void save(std::filesystem::path const& path);
		void save();

		mpp::ParticleEffectSpecification const& specification() const { return mSpecification; }
		mpp::DiagnosticBag const& diagnostics() const { return mDiagnostics; }
		std::filesystem::path const& path() const { return mPath; }
		bool dirty() const { return mDirty; }
		bool hasPath() const { return !mPath.empty(); }
		std::string displayName() const;
	};

	bool runParticleDocumentTests(std::string* failure = nullptr);
}
