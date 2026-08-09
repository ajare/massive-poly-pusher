#pragma once

#include <glm/vec2.hpp>
#include "ProcessFlowModel.h"

namespace pipeline_editor
{
	struct ProcessFlowBounds
	{
		glm::vec2 minimum{ 0.0f };
		glm::vec2 maximum{ 0.0f };
		bool valid{ false };
	};

	struct ProcessFlowTransform
	{
		glm::vec2 pan{ 0.0f };
		float zoom{ 1.0f };
	};

	class ProcessFlowLayout
	{
	public:
		void apply(ProcessFlowModel& model) const;
		ProcessFlowBounds bounds(ProcessFlowModel const& model) const;
		ProcessFlowTransform fitAll(ProcessFlowModel const& model, glm::vec2 viewport, float padding = 40.0f) const;
		static ProcessFlowTransform zoomAroundCursor(ProcessFlowTransform value, glm::vec2 cursor,
		                                             float newZoom);
	};

	void runProcessFlowLayoutTests();
}
