#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <mpp/Diagnostic.h>
#include <mpp/ParticleEffectSpecification.h>
#include <mpp/app/CommandStack.h>
#include <mpp/app/DocumentFile.h>

namespace particle_editor
{
	enum class ParticleSaveResult
	{
		Saved,
		InvalidConfirmationRequired,
		ExternalConflict
	};

	enum class ExternalChangeState
	{
		None,
		ReloadAvailable,
		Conflict
	};

	struct ParticleDocumentComparison
	{
		std::string editorYaml;
		std::string diskYaml;
	};

	class ParticleDocument
	{
		mpp::ParticleEffectSpecification mSpecification;
		mpp::ParticleEffectSpecification mLastValidPreview;
		mpp::DiagnosticBag mDiagnostics;
		mpp::app::CommandStack mCommands;
		mpp::app::DocumentFileRevision mFileRevision;
		std::filesystem::path mPath;
		uint64_t mPreviewRevision{ 0 };
		bool mHasValidPreview{ false };
		bool mForcedDirty{ false };
		bool mPreviewPaused{ false };
		float mPreviewTimeScale{ 1.0f };
		std::string mPreviewFailure;

		void refreshDiagnostics(bool updatePreview = true);

	public:
		ParticleDocument();

		static mpp::ParticleEffectSpecification makeStarterEffect();
		void createNew();
		bool open(std::filesystem::path const& path);
		ParticleSaveResult save(std::filesystem::path const& path, bool allowInvalid = false,
			bool overwriteExternal = false);
		ParticleSaveResult save(bool allowInvalid = false, bool overwriteExternal = false);
		bool reload();
		ExternalChangeState externalChangeState() const;
		void keepEditorVersion();
		ParticleDocumentComparison compareWithDisk() const;

		void executeEdit(std::string name,
			std::function<void(mpp::ParticleEffectSpecification&)> const& edit, bool coalesce = false);
		void endContinuousEdit();
		bool undo();
		bool redo();

		mpp::ParticleEffectSpecification const& specification() const { return mSpecification; }
		mpp::ParticleEffectSpecification const* previewSpecification() const
			{ return mHasValidPreview ? &mLastValidPreview : nullptr; }
		mpp::DiagnosticBag const& diagnostics() const { return mDiagnostics; }
		std::filesystem::path const& path() const { return mPath; }
		bool dirty() const { return mForcedDirty || mCommands.dirty(); }
		bool hasPath() const { return !mPath.empty(); }
		bool canUndo() const { return mCommands.canUndo(); }
		bool canRedo() const { return mCommands.canRedo(); }
		std::string const* undoName() const { return mCommands.undoName(); }
		std::string const* redoName() const { return mCommands.redoName(); }
		size_t commandCount() const { return mCommands.size(); }
		uint64_t previewRevision() const { return mPreviewRevision; }
		bool previewPaused() const { return mPreviewPaused; }
		float previewTimeScale() const { return mPreviewTimeScale; }
		std::string const& previewFailure() const { return mPreviewFailure; }
		void setPreviewPaused(bool paused) { mPreviewPaused = paused; }
		void setPreviewTimeScale(float scale) { mPreviewTimeScale = scale; }
		void setPreviewFailure(std::string failure) { mPreviewFailure = std::move(failure); }
		std::string displayName() const;
	};

	enum class CloseRequestResult
	{
		Closed,
		UnsavedChanges
	};

	class ParticleDocumentTabs
	{
		std::vector<std::unique_ptr<ParticleDocument>> mDocuments;
		size_t mActive{ 0 };

	public:
		explicit ParticleDocumentTabs(bool createInitialDocument = true);
		size_t createNew();
		bool open(std::filesystem::path const& path);
		ParticleDocument* active();
		ParticleDocument const* active() const;
		ParticleDocument& at(size_t index);
		ParticleDocument const& at(size_t index) const;
		size_t size() const { return mDocuments.size(); }
		size_t activeIndex() const { return mActive; }
		void activate(size_t index);
		CloseRequestResult requestClose(size_t index);
		void discardAndClose(size_t index);
		bool hasUnsavedChanges() const;
	};

	bool runParticleDocumentTests(std::string* failure = nullptr);
}
