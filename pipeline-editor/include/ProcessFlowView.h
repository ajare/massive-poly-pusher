#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>

#include "ProcessFlowLayout.h"

namespace pipeline_editor
{
	struct ProcessFlowSelection
	{
		enum class Kind { None, Pass, Image, Import, Material, SceneObject } kind{ Kind::None };
		int index{ -1 };
		std::string materialName;
		int sceneObjectIndex{ -1 };
	};

	struct ProcessFlowHighlight
	{
		int pass{ -1 };
		int image{ -1 };
		int import{ -1 };
		int sceneObject{ -1 };
		std::string materialName;
	};

	class ProcessFlowView
	{
		ProcessFlowFilters mFilters;
		ProcessFlowTransform mTransform;
		ProcessFlowLayout mLayout;
		std::unordered_set<uint64_t> mExpanded;
		uint64_t mFittedRevision{ 0 };
		uint64_t mExpandedPipelineGeneration{ UINT64_MAX };
		uint64_t mExpandedSceneGeneration{ UINT64_MAX };
		bool mFiltersChanged{ true };
		bool mFitRequested{ true };
		bool mRefreshRequested{ false };

	public:
		ProcessFlowFilters const& filters() const { return mFilters; }
		bool consumeFiltersChanged();
		bool consumeRefreshRequested() { bool value = mRefreshRequested; mRefreshRequested = false; return value; }
		void requestFit() { mFitRequested = true; }
		ProcessFlowSelection draw(ProcessFlowModel& model, double sampleAgeSeconds,
		                          ProcessFlowHighlight const& highlight = {});
	};
}
