#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <mpp/Diagnostic.h>
#include <mpp/ParticleEffectSpecification.h>
#include <mpp/app/CommandStack.h>
#include <mpp/app/DocumentFile.h>

#include "ParticleSpatialEditing.h"

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

	struct EmitterEventReference
	{
		size_t emitterIndex{};
		size_t eventIndex{};
		std::string emitterName;
	};

	enum class ParticlePreviewChange
	{
		None,
		Live,
		Structural
	};

	class ParticleDocument
	{
		mpp::ParticleEffectSpecification mSpecification;
		mpp::ParticleEffectSpecification mLastValidPreview;
		std::optional<mpp::ParticleEffectSpecification> mPendingPreview;
		mpp::DiagnosticBag mDiagnostics;
		mpp::app::CommandStack mCommands;
		mpp::app::DocumentFileRevision mFileRevision;
		std::filesystem::path mPath;
		std::chrono::steady_clock::time_point mPreviewDeadline{};
		uint64_t mPreviewRevision{ 0 };
		size_t mSelectedEmitter{ 0 };
		std::optional<size_t> mSelectedChildEffect;
		ParticlePreviewChange mPendingPreviewChange{ ParticlePreviewChange::None };
		ParticlePreviewChange mPublishedPreviewChange{ ParticlePreviewChange::None };
		ParticlePreviewChange mAppliedCommandChange{ ParticlePreviewChange::Structural };
		bool mHasValidPreview{ false };
		bool mForcedDirty{ false };
		bool mPreviewPaused{ false };
		float mPreviewTimeScale{ 1.0f };
		std::string mPreviewFailure;

		void refreshDiagnostics(bool updatePreview = true,
			ParticlePreviewChange change = ParticlePreviewChange::Structural);
		void publishPreview(ParticlePreviewChange change);
		static void maintainInvariants(mpp::ParticleEffectSpecification& specification);

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
			std::function<void(mpp::ParticleEffectSpecification&)> const& edit, bool coalesce = false,
			ParticlePreviewChange change = ParticlePreviewChange::Structural);
		void endContinuousEdit();
		bool undo();
		bool redo();

		size_t addEmitterTemplate();
		size_t duplicateEmitterTemplate(size_t index);
		void renameEmitterTemplate(size_t index, std::string name, bool coalesce = false);
		void moveEmitterTemplate(size_t from, size_t to);
		std::vector<EmitterEventReference> emitterEventReferences(size_t index) const;
		bool removeEmitterTemplate(size_t index, bool removeReferencingRules = false);
		void selectEmitterTemplate(size_t index);

		size_t addEventRule(size_t emitterIndex);
		size_t duplicateEventRule(size_t emitterIndex, size_t eventIndex);
		void moveEventRule(size_t emitterIndex, size_t from, size_t to);
		void removeEventRule(size_t emitterIndex, size_t eventIndex);
		size_t addChildEffect();
		size_t duplicateChildEffect(size_t index);
		void moveChildEffect(size_t from, size_t to);
		void removeChildEffect(size_t index);
		void selectChildEffect(size_t index);
		std::optional<SpatialTarget> selectedSpatialTarget() const;
		void selectSpatialTarget(SpatialTarget target);
		glm::mat4 selectedTransform() const;
		void setSelectedTransform(glm::mat4 const& transform, bool continuous = false);

		size_t addScalarCurveKey(size_t emitterIndex, mpp::ParticleScalarCurve curve,
			float time, float value);
		void editScalarCurveKey(size_t emitterIndex, mpp::ParticleScalarCurve curve,
			size_t keyIndex, float time, float value, bool continuous = false);
		void removeScalarCurveKey(size_t emitterIndex, mpp::ParticleScalarCurve curve, size_t keyIndex);
		void setScalarCurveDefault(size_t emitterIndex, mpp::ParticleScalarCurve curve,
			float value, bool continuous = false);
		size_t addColourGradientKey(size_t emitterIndex, float time, std::array<float, 3> colour);
		void editColourGradientKey(size_t emitterIndex, size_t keyIndex, float time,
			std::array<float, 3> colour, bool continuous = false);
		void removeColourGradientKey(size_t emitterIndex, size_t keyIndex);
		void setColourGradientDefault(size_t emitterIndex, std::array<float, 3> colour,
			bool continuous = false);

		bool hasSelectedEmitterTemplate() const;
		bool hasSelectedChildEffect() const { return mSelectedChildEffect && *mSelectedChildEffect < mSpecification.childEffects.size(); }
		size_t selectedChildEffect() const;
		size_t selectedEmitterTemplate() const;
		static std::string uniqueEmitterTemplateName(mpp::ParticleEffectSpecification const& specification,
			std::string requested, std::optional<size_t> ignoredIndex = std::nullopt);

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
		ParticlePreviewChange previewChange() const { return mPublishedPreviewChange; }
		bool publishPreviewIfDue(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
		bool publishPreviewNow();
		bool previewUpdatePending() const { return mPendingPreview.has_value(); }
		std::chrono::steady_clock::time_point previewDeadline() const { return mPreviewDeadline; }
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
