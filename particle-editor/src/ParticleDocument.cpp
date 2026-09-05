#include "ParticleDocument.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include <mpp/ParticleEffectValidator.h>
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

		std::string canonicalYaml(mpp::ParticleEffectSpecification specification)
		{
			specification.version = 2u;
			auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
			auto temporary = std::filesystem::temp_directory_path() /
				("mpp-particle-editor-comparison-" + std::to_string(nonce) + ".particle.yaml");
			try
			{
				mpp::resource_parsers::ParticleEffectSerializer::toFile(specification, temporary.string());
				auto result = readText(temporary);
				std::error_code ignored;
				std::filesystem::remove(temporary, ignored);
				return result;
			}
			catch (...)
			{
				std::error_code ignored;
				std::filesystem::remove(temporary, ignored);
				std::filesystem::remove(temporary.string() + ".tmp", ignored);
				throw;
			}
		}

		ParticlePreviewChange combinePreviewChange(ParticlePreviewChange left, ParticlePreviewChange right)
		{
			return static_cast<ParticlePreviewChange>(std::max(uint32_t(left), uint32_t(right)));
		}

		class ReplaceSpecificationCommand final : public mpp::app::EditorCommand
		{
			std::string mName;
			mpp::ParticleEffectSpecification* mTarget;
			ParticlePreviewChange* mAppliedChange;
			mpp::ParticleEffectSpecification mBefore;
			mpp::ParticleEffectSpecification mAfter;
			ParticlePreviewChange mChange;

		public:
			ReplaceSpecificationCommand(std::string name, mpp::ParticleEffectSpecification* target,
				ParticlePreviewChange* appliedChange, mpp::ParticleEffectSpecification before,
				mpp::ParticleEffectSpecification after, ParticlePreviewChange change)
				: mName(std::move(name)), mTarget(target), mAppliedChange(appliedChange), mBefore(std::move(before)),
				  mAfter(std::move(after)), mChange(change) {}

			std::string const& name() const override { return mName; }
			void execute() override { *mTarget = mAfter; *mAppliedChange = mChange; }
			void undo() override { *mTarget = mBefore; *mAppliedChange = mChange; }
			bool merge(mpp::app::EditorCommand const& other) override
			{
				auto replacement = dynamic_cast<ReplaceSpecificationCommand const*>(&other);
				if (!replacement || replacement->mTarget != mTarget || replacement->mName != mName) return false;
				mAfter = replacement->mAfter;
				mChange = combinePreviewChange(mChange, replacement->mChange);
				return true;
			}
		};
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

	void ParticleDocument::maintainInvariants(mpp::ParticleEffectSpecification& specification)
	{
		uint64_t total = 0u;
		std::unordered_set<std::string> names;
		for (auto& emitter : specification.emitterTemplates)
		{
			auto base = emitter.name.empty() ? std::string("Emitter") : emitter.name;
			auto candidate = base;
			for (uint32_t suffix = 2u; !names.emplace(candidate).second; ++suffix)
				candidate = base + " " + std::to_string(suffix);
			emitter.name = std::move(candidate);
			total += emitter.value.simulation.shapeSeedModulesBudget[3];
		}
		specification.maximumParticleCount = uint32_t(std::min<uint64_t>(total,
			std::numeric_limits<uint32_t>::max()));
	}

	void ParticleDocument::publishPreview(ParticlePreviewChange change)
	{
		if (!mPendingPreview) return;
		mLastValidPreview = std::move(*mPendingPreview);
		mPendingPreview.reset();
		mPendingPreviewChange = ParticlePreviewChange::None;
		mPublishedPreviewChange = change;
		mHasValidPreview = true;
		++mPreviewRevision;
	}

	void ParticleDocument::refreshDiagnostics(bool updatePreview, ParticlePreviewChange change)
	{
		mDiagnostics = mpp::ParticleEffectValidator::validate(mSpecification, mPath.string());
		if (!updatePreview) return;
		if (mDiagnostics.hasErrors())
		{
			mPendingPreview.reset();
			mPendingPreviewChange = ParticlePreviewChange::None;
			return;
		}
		mPendingPreview = mSpecification;
		mPendingPreviewChange = combinePreviewChange(mPendingPreviewChange, change);
		mPreviewDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(150);
	}

	void ParticleDocument::createNew()
	{
		mSpecification = makeStarterEffect();
		maintainInvariants(mSpecification);
		mPath.clear();
		mFileRevision = {};
		mCommands.clear();
		mCommands.markSavePoint();
		mPreviewRevision = 0u;
		mSelectedEmitter = 0u;
		mPendingPreview.reset();
		mPendingPreviewChange = ParticlePreviewChange::None;
		mPublishedPreviewChange = ParticlePreviewChange::None;
		mHasValidPreview = false;
		mForcedDirty = true;
		mPreviewPaused = false;
		mPreviewTimeScale = 1.0f;
		mPreviewFailure.clear();
		refreshDiagnostics(true, ParticlePreviewChange::Structural);
		publishPreview(ParticlePreviewChange::Structural);
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
		maintainInvariants(mSpecification);
		mPath = std::move(normalised);
		mFileRevision = mpp::app::captureDocumentFileRevision(mPath);
		mCommands.clear();
		mCommands.markSavePoint();
		mPreviewRevision = 0u;
		mSelectedEmitter = 0u;
		mPendingPreview.reset();
		mPendingPreviewChange = ParticlePreviewChange::None;
		mPublishedPreviewChange = ParticlePreviewChange::None;
		mHasValidPreview = false;
		mForcedDirty = upgraded;
		mPreviewPaused = false;
		mPreviewTimeScale = 1.0f;
		mPreviewFailure.clear();
		refreshDiagnostics(true, ParticlePreviewChange::Structural);
		if (!mDiagnostics.hasErrors()) publishPreview(ParticlePreviewChange::Structural);
		return !mDiagnostics.hasErrors();
	}

	ParticleSaveResult ParticleDocument::save(std::filesystem::path const& path, bool allowInvalid,
		bool overwriteExternal)
	{
		if (path.empty()) throw std::invalid_argument("Particle effect save path is empty.");
		auto normalised = mpp::app::normaliseDocumentPath(canonicalParticleEffectPath(path));
		if (!mPath.empty() && normalised == mPath &&
			mpp::app::documentFileChanged(mPath, mFileRevision) && !overwriteExternal)
			return ParticleSaveResult::ExternalConflict;

		mSpecification.version = 2u;
		refreshDiagnostics(false);
		if (mDiagnostics.hasErrors() && !allowInvalid)
			return ParticleSaveResult::InvalidConfirmationRequired;

		if (!normalised.parent_path().empty()) std::filesystem::create_directories(normalised.parent_path());
		mpp::resource_parsers::ParticleEffectSerializer::toFile(mSpecification, normalised.string());
		mPath = std::move(normalised);
		mFileRevision = mpp::app::captureDocumentFileRevision(mPath);
		mCommands.markSavePoint();
		mForcedDirty = false;
		refreshDiagnostics(false);
		return ParticleSaveResult::Saved;
	}

	ParticleSaveResult ParticleDocument::save(bool allowInvalid, bool overwriteExternal)
	{
		if (mPath.empty()) throw std::logic_error("The particle effect does not have a save path.");
		return save(mPath, allowInvalid, overwriteExternal);
	}

	bool ParticleDocument::reload()
	{
		if (mPath.empty()) return false;
		auto path = mPath;
		return open(path);
	}

	ExternalChangeState ParticleDocument::externalChangeState() const
	{
		if (mPath.empty() || !mpp::app::documentFileChanged(mPath, mFileRevision))
			return ExternalChangeState::None;
		return dirty() ? ExternalChangeState::Conflict : ExternalChangeState::ReloadAvailable;
	}

	void ParticleDocument::keepEditorVersion()
	{
		if (!mPath.empty()) mFileRevision = mpp::app::captureDocumentFileRevision(mPath);
	}

	ParticleDocumentComparison ParticleDocument::compareWithDisk() const
	{
		if (mPath.empty()) throw std::logic_error("The particle effect does not have a disk version.");
		return { canonicalYaml(mSpecification), readText(mPath) };
	}

	void ParticleDocument::executeEdit(std::string name,
		std::function<void(mpp::ParticleEffectSpecification&)> const& edit, bool coalesce,
		ParticlePreviewChange change)
	{
		if (!edit) throw std::invalid_argument("Particle document edit is empty.");
		auto after = mSpecification;
		edit(after);
		maintainInvariants(after);
		mCommands.execute(std::make_unique<ReplaceSpecificationCommand>(std::move(name), &mSpecification,
			&mAppliedCommandChange, mSpecification, std::move(after), change), coalesce);
		refreshDiagnostics(true, mAppliedCommandChange);
	}

	void ParticleDocument::endContinuousEdit()
	{
		mCommands.endCoalescing();
	}

	bool ParticleDocument::undo()
	{
		if (!mCommands.undo()) return false;
		refreshDiagnostics(true, mAppliedCommandChange);
		return true;
	}

	bool ParticleDocument::redo()
	{
		if (!mCommands.redo()) return false;
		refreshDiagnostics(true, mAppliedCommandChange);
		return true;
	}

	std::string ParticleDocument::uniqueEmitterTemplateName(
		mpp::ParticleEffectSpecification const& specification, std::string requested,
		std::optional<size_t> ignoredIndex)
	{
		if (requested.empty()) requested = "Emitter";
		auto available = [&](std::string const& candidate)
		{
			for (size_t index = 0; index < specification.emitterTemplates.size(); ++index)
				if ((!ignoredIndex || index != *ignoredIndex) && specification.emitterTemplates[index].name == candidate)
					return false;
			return true;
		};
		if (available(requested)) return requested;
		for (uint32_t suffix = 2u;; ++suffix)
		{
			auto candidate = requested + " " + std::to_string(suffix);
			if (available(candidate)) return candidate;
		}
	}

	size_t ParticleDocument::addEmitterTemplate()
	{
		auto name = uniqueEmitterTemplateName(mSpecification, "Emitter");
		auto emitter = makeStarterEffect().emitterTemplates.front();
		emitter.name = name;
		executeEdit("Add emitter template", [emitter = std::move(emitter)](auto& effect) mutable
			{ effect.emitterTemplates.push_back(std::move(emitter)); });
		mSelectedEmitter = mSpecification.emitterTemplates.size() - 1u;
		return mSelectedEmitter;
	}

	size_t ParticleDocument::duplicateEmitterTemplate(size_t index)
	{
		if (index >= mSpecification.emitterTemplates.size())
			throw std::out_of_range("Emitter-template index is out of range.");
		auto emitter = mSpecification.emitterTemplates[index];
		emitter.name = uniqueEmitterTemplateName(mSpecification, emitter.name + " Copy");
		executeEdit("Duplicate emitter template", [index, emitter = std::move(emitter)](auto& effect) mutable
			{ effect.emitterTemplates.insert(effect.emitterTemplates.begin() + index + 1u, std::move(emitter)); });
		mSelectedEmitter = index + 1u;
		return mSelectedEmitter;
	}

	void ParticleDocument::renameEmitterTemplate(size_t index, std::string name, bool coalesce)
	{
		if (index >= mSpecification.emitterTemplates.size())
			throw std::out_of_range("Emitter-template index is out of range.");
		name = uniqueEmitterTemplateName(mSpecification, std::move(name), index);
		auto oldName = mSpecification.emitterTemplates[index].name;
		executeEdit("Rename emitter template", [index, oldName = std::move(oldName), name = std::move(name)](auto& effect)
		{
			effect.emitterTemplates[index].name = name;
			for (auto& emitter : effect.emitterTemplates)
				for (auto& event : emitter.events)
					if (event.targetEmitter == oldName) event.targetEmitter = name;
		}, coalesce);
	}

	void ParticleDocument::moveEmitterTemplate(size_t from, size_t to)
	{
		if (from >= mSpecification.emitterTemplates.size() || to >= mSpecification.emitterTemplates.size())
			throw std::out_of_range("Emitter-template index is out of range.");
		if (from == to) return;
		executeEdit("Reorder emitter template", [from, to](auto& effect)
		{
			auto emitter = std::move(effect.emitterTemplates[from]);
			effect.emitterTemplates.erase(effect.emitterTemplates.begin() + from);
			effect.emitterTemplates.insert(effect.emitterTemplates.begin() + to, std::move(emitter));
		});
		mSelectedEmitter = to;
	}

	void ParticleDocument::removeEmitterTemplate(size_t index)
	{
		if (index >= mSpecification.emitterTemplates.size())
			throw std::out_of_range("Emitter-template index is out of range.");
		executeEdit("Remove emitter template", [index](auto& effect)
			{ effect.emitterTemplates.erase(effect.emitterTemplates.begin() + index); });
		mSelectedEmitter = mSpecification.emitterTemplates.empty() ? 0u :
			std::min(index, mSpecification.emitterTemplates.size() - 1u);
	}

	void ParticleDocument::selectEmitterTemplate(size_t index)
	{
		if (index >= mSpecification.emitterTemplates.size())
			throw std::out_of_range("Emitter-template index is out of range.");
		if (index != mSelectedEmitter) endContinuousEdit();
		mSelectedEmitter = index;
	}

	bool ParticleDocument::hasSelectedEmitterTemplate() const
	{
		return mSelectedEmitter < mSpecification.emitterTemplates.size();
	}

	size_t ParticleDocument::selectedEmitterTemplate() const
	{
		if (!hasSelectedEmitterTemplate()) throw std::logic_error("No emitter template is selected.");
		return mSelectedEmitter;
	}

	bool ParticleDocument::publishPreviewIfDue(std::chrono::steady_clock::time_point now)
	{
		if (!mPendingPreview || now < mPreviewDeadline) return false;
		auto change = mPendingPreviewChange;
		publishPreview(change);
		return true;
	}

	bool ParticleDocument::publishPreviewNow()
	{
		if (!mPendingPreview) return false;
		auto change = mPendingPreviewChange;
		publishPreview(change);
		return true;
	}

	std::string ParticleDocument::displayName() const
	{
		return mPath.empty() ? "Untitled Particle Effect" : mPath.filename().string();
	}

	ParticleDocumentTabs::ParticleDocumentTabs(bool createInitialDocument)
	{
		if (createInitialDocument) createNew();
	}

	size_t ParticleDocumentTabs::createNew()
	{
		if (auto document = active()) document->endContinuousEdit();
		mDocuments.push_back(std::make_unique<ParticleDocument>());
		mActive = mDocuments.size() - 1;
		return mActive;
	}

	bool ParticleDocumentTabs::open(std::filesystem::path const& path)
	{
		for (size_t index = 0; index < mDocuments.size(); ++index)
			if (mDocuments[index]->hasPath() && mDocuments[index]->path() == mpp::app::normaliseDocumentPath(path))
			{
				activate(index);
				return true;
			}
		auto document = std::make_unique<ParticleDocument>();
		if (!document->open(path)) return false;
		if (auto current = active()) current->endContinuousEdit();
		mDocuments.push_back(std::move(document));
		mActive = mDocuments.size() - 1;
		return true;
	}

	ParticleDocument* ParticleDocumentTabs::active()
	{
		return mDocuments.empty() ? nullptr : mDocuments[mActive].get();
	}

	ParticleDocument const* ParticleDocumentTabs::active() const
	{
		return mDocuments.empty() ? nullptr : mDocuments[mActive].get();
	}

	ParticleDocument& ParticleDocumentTabs::at(size_t index)
	{
		if (index >= mDocuments.size()) throw std::out_of_range("Particle document tab index is out of range.");
		return *mDocuments[index];
	}

	ParticleDocument const& ParticleDocumentTabs::at(size_t index) const
	{
		if (index >= mDocuments.size()) throw std::out_of_range("Particle document tab index is out of range.");
		return *mDocuments[index];
	}

	void ParticleDocumentTabs::activate(size_t index)
	{
		if (index >= mDocuments.size()) throw std::out_of_range("Particle document tab index is out of range.");
		if (index != mActive && !mDocuments.empty()) mDocuments[mActive]->endContinuousEdit();
		mActive = index;
	}

	CloseRequestResult ParticleDocumentTabs::requestClose(size_t index)
	{
		if (at(index).dirty()) return CloseRequestResult::UnsavedChanges;
		discardAndClose(index);
		return CloseRequestResult::Closed;
	}

	void ParticleDocumentTabs::discardAndClose(size_t index)
	{
		if (index >= mDocuments.size()) throw std::out_of_range("Particle document tab index is out of range.");
		mDocuments.erase(mDocuments.begin() + index);
		if (mDocuments.empty()) mActive = 0;
		else if (mActive > index) --mActive;
		else if (mActive >= mDocuments.size()) mActive = mDocuments.size() - 1;
	}

	bool ParticleDocumentTabs::hasUnsavedChanges() const
	{
		for (auto const& document : mDocuments) if (document->dirty()) return true;
		return false;
	}

	bool runParticleDocumentTests(std::string* failure)
	{
		auto fail = [&](std::string message)
		{
			if (failure) *failure = std::move(message);
			return false;
		};
		std::filesystem::path root;
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

			ParticleDocument hierarchy;
			auto added = hierarchy.addEmitterTemplate();
			auto duplicated = hierarchy.duplicateEmitterTemplate(added);
			hierarchy.renameEmitterTemplate(duplicated, "Emitter");
			hierarchy.moveEmitterTemplate(duplicated, 0u);
			hierarchy.removeEmitterTemplate(0u);
			if (hierarchy.specification().emitterTemplates.size() != 2u ||
				hierarchy.specification().maximumParticleCount != 2048u ||
				hierarchy.specification().emitterTemplates[0].name == hierarchy.specification().emitterTemplates[1].name ||
				!hierarchy.hasSelectedEmitterTemplate())
				return fail("emitter-template hierarchy operations did not maintain selection, unique names, and the derived budget");
			if (!hierarchy.undo() || hierarchy.specification().emitterTemplates.size() != 3u || !hierarchy.redo() ||
				hierarchy.specification().emitterTemplates.size() != 2u)
				return fail("emitter-template hierarchy removal was not undoable and redoable");

			ParticleDocument debounced;
			auto const initialPreviewRevision = debounced.previewRevision();
			debounced.executeEdit("Live tint", [](auto& effect)
				{ effect.emitterTemplates[0].value.appearance.tintAndAlpha[0] = 0.5f; }, false,
				ParticlePreviewChange::Live);
			if (!debounced.previewUpdatePending() || debounced.previewRevision() != initialPreviewRevision ||
				debounced.publishPreviewIfDue(debounced.previewDeadline() - std::chrono::milliseconds(1)) ||
				!debounced.publishPreviewIfDue(debounced.previewDeadline()) ||
				debounced.previewRevision() != initialPreviewRevision + 1u ||
				debounced.previewChange() != ParticlePreviewChange::Live ||
				debounced.previewSpecification()->emitterTemplates[0].value.appearance.tintAndAlpha[0] != 0.5f)
				return fail("valid live edits did not publish exactly once after the preview debounce");
			debounced.executeEdit("Invalid range", [](auto& effect)
				{ effect.emitterTemplates[0].value.simulation.initialVelocityMin[0] = 10.0f; });
			if (!debounced.diagnostics().hasErrors() || debounced.previewUpdatePending() ||
				debounced.previewSpecification()->emitterTemplates[0].value.simulation.initialVelocityMin[0] == 10.0f)
				return fail("an invalid range replaced or queued over the last valid preview");

			auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
			root = std::filesystem::temp_directory_path() / ("mpp-particle-editor-tests-" + std::to_string(nonce));
			std::filesystem::create_directories(root);

			auto hierarchyPath = root / "hierarchy.particle.yaml";
			if (hierarchy.save(hierarchyPath) != ParticleSaveResult::Saved)
				return fail("the edited emitter-template hierarchy did not save");
			ParticleDocument restoredHierarchy;
			if (!restoredHierarchy.open(hierarchyPath) || restoredHierarchy.specification().emitterTemplates.size() != 2u ||
				restoredHierarchy.specification().emitterTemplates[0].name != hierarchy.specification().emitterTemplates[0].name ||
				restoredHierarchy.specification().emitterTemplates[1].name != hierarchy.specification().emitterTemplates[1].name ||
				restoredHierarchy.specification().maximumParticleCount != hierarchy.specification().maximumParticleCount)
				return fail("the emitter-template hierarchy did not round-trip through canonical YAML");

			ParticleDocument properties;
			for (uint32_t shape = uint32_t(mpp::ParticleSpawnShape::Point);
				shape <= uint32_t(mpp::ParticleSpawnShape::Cone); ++shape)
				properties.executeEdit("Spawn shape", [shape](auto& effect)
					{ effect.emitterTemplates[0].value.simulation.shapeSeedModulesBudget[0] = shape; });
			properties.executeEdit("Core emitter properties", [](auto& effect)
			{
				auto& authored = effect.emitterTemplates[0];
				auto& simulation = authored.value.simulation;
				auto& appearance = authored.value.appearance;
				simulation.shapeParameters = { 1.25f, 2.5f, 0.0f, 0.0f };
				simulation.shapeSeedModulesBudget[1] = 987u;
				simulation.shapeSeedModulesBudget[3] = 333u;
				simulation.emissionState = { 1u, 0u, 77u, 0u };
				simulation.initialVelocityMin = { -3.0f, -2.0f, -1.0f, 0.0f };
				simulation.initialVelocityMax = { 1.0f, 2.0f, 3.0f, 0.0f };
				simulation.colourMin = { 0.1f, 0.2f, 0.3f, 0.4f };
				simulation.colourMax = { 0.6f, 0.7f, 0.8f, 0.9f };
				simulation.lifetimeSizeRanges = { 0.5f, 4.0f, 0.05f, 1.5f };
				simulation.rotationRanges = { -2.0f, 2.0f, -5.0f, 5.0f };
				authored.albedoTexture = "Textures/Particles/Glow";
				appearance.tintAndAlpha = { 1.5f, 0.75f, 0.25f, 0.65f };
				appearance.appearance[0] = 4.5f;
				appearance.appearance[1] = 0.35f;
				appearance.modes[2] = uint32_t(mpp::ParticleBillboardMode::VelocityStretched);
				appearance.modes[3] = uint32_t(mpp::ParticleBlendClass::Alpha);
				appearance.sorting[0] = uint32_t(mpp::ParticleSortMode::BackToFront);
			});
			if (!properties.undo() || properties.specification().maximumParticleCount != 1024u ||
				!properties.redo() || properties.specification().maximumParticleCount != 333u)
				return fail("representative emitter property edits did not round-trip through undo and redo");
			auto propertyPath = root / "properties.particle.yaml";
			if (properties.specification().maximumParticleCount != 333u ||
				properties.save(propertyPath) != ParticleSaveResult::Saved)
				return fail("direct emitter budgets did not derive the read-only particle effect maximum");
			ParticleDocument restoredProperties;
			if (!restoredProperties.open(propertyPath))
				return fail("representative emitter properties did not reopen through the production parser");
			auto const& propertyEmitter = restoredProperties.specification().emitterTemplates[0];
			auto const& propertySimulation = propertyEmitter.value.simulation;
			auto const& propertyAppearance = propertyEmitter.value.appearance;
			if (propertySimulation.shapeSeedModulesBudget != std::array<uint32_t, 4>{ 6u, 987u, 0u, 333u } ||
				propertySimulation.shapeParameters != std::array<float, 4>{ 1.25f, 2.5f, 0.0f, 0.0f } ||
				propertySimulation.emissionState[0] != 1u || propertySimulation.emissionState[1] != 0u ||
				propertySimulation.emissionState[2] != 77u ||
				propertySimulation.initialVelocityMin != std::array<float, 4>{ -3.0f, -2.0f, -1.0f, 0.0f } ||
				propertySimulation.colourMax != std::array<float, 4>{ 0.6f, 0.7f, 0.8f, 0.9f } ||
				propertySimulation.lifetimeSizeRanges != std::array<float, 4>{ 0.5f, 4.0f, 0.05f, 1.5f } ||
				propertySimulation.rotationRanges != std::array<float, 4>{ -2.0f, 2.0f, -5.0f, 5.0f } ||
				propertyEmitter.albedoTexture != "Textures/Particles/Glow" ||
				propertyAppearance.tintAndAlpha != std::array<float, 4>{ 1.5f, 0.75f, 0.25f, 0.65f } ||
				propertyAppearance.appearance[0] != 4.5f || propertyAppearance.appearance[1] != 0.35f ||
				propertyAppearance.modes[2] != uint32_t(mpp::ParticleBillboardMode::VelocityStretched) ||
				propertyAppearance.modes[3] != uint32_t(mpp::ParticleBlendClass::Alpha) ||
				propertyAppearance.sorting[0] != uint32_t(mpp::ParticleSortMode::BackToFront))
				return fail("spawn and core billboard appearance properties did not round-trip through canonical YAML");

			auto selectedPath = root / "canonical.yaml";
			auto path = root / "canonical.particle.yaml";
			ParticleDocument document;
			if (document.save(selectedPath) != ParticleSaveResult::Saved)
				return fail("a valid particle effect did not save");
			if (document.path() != mpp::app::normaliseDocumentPath(path))
				return fail("the editor document did not enforce the canonical .particle.yaml suffix");
			auto canonical = readText(path);
			if (canonical.find("version: 2") == std::string::npos ||
				canonical.find("Bounds:") == std::string::npos || canonical.find("seed: 17") == std::string::npos)
				return fail("the production serializer omitted required starter fields");
			if (document.save() != ParticleSaveResult::Saved || readText(path) != canonical ||
				std::filesystem::exists(path.string() + ".tmp"))
				return fail("atomic canonical save was not stable");
			ParticleDocument restored;
			if (!restored.open(path) || restored.specification().version != 2u || restored.dirty() ||
				restored.specification().emitterTemplates.front().value.simulation.shapeSeedModulesBudget[1] != 17u)
				return fail("the editor document did not open its production-serialized particle effect");
			auto restoredPath = restored.path();
			if (restored.open(path.string() + ".missing") || restored.path() != restoredPath ||
				restored.specification().name != "Untitled Particle Effect" || !restored.diagnostics().hasErrors())
				return fail("a failed production parse replaced the active editor document");

			ParticleDocument history;
			history.executeEdit("Emission rate", [](auto& effect)
				{ effect.emitterTemplates.front().value.simulation.emissionRateAndPadding[0] = 21.0f; }, true);
			history.executeEdit("Emission rate", [](auto& effect)
				{ effect.emitterTemplates.front().value.simulation.emissionRateAndPadding[0] = 22.0f; }, true);
			history.executeEdit("Emission rate", [](auto& effect)
				{ effect.emitterTemplates.front().value.simulation.emissionRateAndPadding[0] = 23.0f; }, true);
			history.endContinuousEdit();
			if (history.commandCount() != 1u || !history.undo() ||
				history.specification().emitterTemplates.front().value.simulation.emissionRateAndPadding[0] != 20.0f ||
				!history.redo() || history.specification().emitterTemplates.front().value.simulation.emissionRateAndPadding[0] != 23.0f)
				return fail("particle document continuous edits did not coalesce into one undoable command");

			ParticleDocumentTabs tabs;
			tabs.at(0).executeEdit("Rename", [](auto& effect) { effect.name = "First"; });
			tabs.at(0).setPreviewPaused(true);
			tabs.at(0).setPreviewFailure("first preview state");
			if (tabs.at(0).save(root / "first.particle.yaml") != ParticleSaveResult::Saved)
				return fail("the first particle tab could not establish its own path");
			tabs.createNew();
			if (tabs.at(1).save(root / "second.particle.yaml") != ParticleSaveResult::Saved)
				return fail("the second particle tab could not establish its own path");
			tabs.at(1).executeEdit("Invalidate", [](auto& effect)
				{ effect.emitterTemplates.front().value.simulation.lifetimeSizeRanges = { 2.0f, 1.0f, 0.12f, 0.22f }; });
			if (tabs.size() != 2u || tabs.at(0).path() == tabs.at(1).path() ||
				tabs.at(0).specification().name != "First" || !tabs.at(0).canUndo() || tabs.at(0).dirty() ||
				!tabs.at(1).canUndo() || !tabs.at(1).dirty() || tabs.at(0).diagnostics().hasErrors() ||
				!tabs.at(1).diagnostics().hasErrors() || !tabs.at(0).previewPaused() || tabs.at(1).previewPaused() ||
				tabs.at(0).previewFailure() != "first preview state" || !tabs.at(1).previewFailure().empty() ||
				!tabs.at(1).previewSpecification() || tabs.at(1).previewSpecification()->maximumParticleCount != 1024u)
				return fail("particle tabs did not isolate path, dirty state, history, diagnostics, and preview state");
			if (tabs.requestClose(1) != CloseRequestResult::UnsavedChanges || !tabs.hasUnsavedChanges())
				return fail("closing or exiting did not surface unsaved particle documents");

			ParticleDocument invalid;
			auto validPreviewRevision = invalid.previewRevision();
			invalid.executeEdit("Invalidate lifetime", [](auto& effect)
				{ effect.emitterTemplates.front().value.simulation.lifetimeSizeRanges = { 3.0f, 1.0f, 0.12f, 0.22f }; });
			auto invalidPath = root / "invalid.particle.yaml";
			if (invalid.save(invalidPath) != ParticleSaveResult::InvalidConfirmationRequired ||
				std::filesystem::exists(invalidPath) || invalid.previewRevision() != validPreviewRevision ||
				!invalid.previewSpecification() || invalid.previewSpecification()->maximumParticleCount != 1024u)
				return fail("an invalid edit replaced the valid preview or saved without confirmation");
			if (invalid.save(invalidPath, true) != ParticleSaveResult::Saved || !std::filesystem::exists(invalidPath) ||
				!mpp::resource_parsers::ParticleEffectParser::fromFile(invalidPath.string()).diagnostics.hasErrors())
				return fail("a confirmed invalid particle effect was not preserved on disk");

			ParticleDocument conflict;
			auto conflictPath = root / "conflict.particle.yaml";
			if (conflict.save(conflictPath) != ParticleSaveResult::Saved)
				return fail("could not prepare revision-conflict test");
			conflict.executeEdit("Rename", [](auto& effect) { effect.name = "Editor Version"; });
			auto diskVersion = ParticleDocument::makeStarterEffect();
			diskVersion.name = "Disk Version";
			mpp::resource_parsers::ParticleEffectSerializer::toFile(diskVersion, conflictPath.string());
			auto externalYaml = readText(conflictPath);
			if (conflict.externalChangeState() != ExternalChangeState::Conflict ||
				conflict.save() != ParticleSaveResult::ExternalConflict || readText(conflictPath) != externalYaml)
				return fail("a dirty revision conflict silently overwrote an external change");
			auto comparison = conflict.compareWithDisk();
			if (comparison.editorYaml.find("Editor Version") == std::string::npos ||
				comparison.diskYaml.find("Disk Version") == std::string::npos)
				return fail("revision conflict comparison did not expose both versions");
			conflict.keepEditorVersion();
			if (conflict.save() != ParticleSaveResult::Saved || readText(conflictPath).find("Editor Version") == std::string::npos)
				return fail("keeping the editor version did not permit an explicit save");
			diskVersion.name = "Clean Reload";
			mpp::resource_parsers::ParticleEffectSerializer::toFile(diskVersion, conflictPath.string());
			if (conflict.externalChangeState() != ExternalChangeState::ReloadAvailable || !conflict.reload() ||
				conflict.specification().name != "Clean Reload" || conflict.dirty())
				return fail("a clean externally changed particle effect could not reload");

			std::error_code ignored;
			std::filesystem::remove_all(root, ignored);
			return true;
		}
		catch (std::exception const& error)
		{
			std::error_code ignored;
			if (!root.empty()) std::filesystem::remove_all(root, ignored);
			return fail(error.what());
		}
	}
}
