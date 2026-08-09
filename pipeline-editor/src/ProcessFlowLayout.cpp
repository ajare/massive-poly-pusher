#include "ProcessFlowLayout.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace pipeline_editor
{
	void ProcessFlowLayout::apply(ProcessFlowModel& model) const
	{
		std::vector<ProcessFlowNode*> spine, disabled, resources;
		for (auto& node : model.nodes)
		{
			node.size.x = node.kind == ProcessFlowNodeKind::BatchSubmission ? 230.0f : 220.0f;
			node.size.y = node.kind == ProcessFlowNodeKind::BatchSubmission && node.expanded
			                  ? 82.0f + 22.0f * (float)std::max<size_t>(1, node.sceneObjects.size())
			                  : 78.0f;
			if (node.mainSpine) spine.push_back(&node);
			else if (node.resourceCategory != ProcessFlowResourceCategory::None) resources.push_back(&node);
			else disabled.push_back(&node);
		}
		std::stable_sort(spine.begin(), spine.end(), [](auto left, auto right)
		{
			if (left->sequence && right->sequence && left->sequence != right->sequence) return left->sequence < right->sequence;
			if (left->actualPosition != right->actualPosition && left->actualPosition >= 0 && right->actualPosition >= 0)
				return left->actualPosition < right->actualPosition;
			return left->layoutRank < right->layoutRank;
		});
		float x = 0.0f;
		for (auto* node : spine) { node->position = {x, 0.0f}; x += node->size.x + 72.0f; }
		auto xForRank = [&](float rank)
		{
			if (spine.empty()) return rank * 292.0f;
			auto nearest = *std::min_element(spine.begin(), spine.end(), [&](auto left, auto right)
			{ return std::abs(left->layoutRank - rank) < std::abs(right->layoutRank - rank); });
			return nearest->position.x + (rank > nearest->layoutRank ? nearest->size.x * 0.55f : 0.0f);
		};
		std::stable_sort(disabled.begin(), disabled.end(), [](auto left, auto right)
		{ return std::tie(left->authoredPosition, left->semanticKey) < std::tie(right->authoredPosition, right->semanticKey); });
		for (size_t index = 0; index < disabled.size(); ++index)
			disabled[index]->position = {xForRank(disabled[index]->layoutRank), 170.0f + (float)index * 104.0f};
		std::stable_sort(resources.begin(), resources.end(), [](auto left, auto right)
		{
			if (left->layoutRank != right->layoutRank) return left->layoutRank < right->layoutRank;
			return left->semanticKey < right->semanticKey;
		});
		for (size_t index = 0; index < resources.size(); ++index)
			resources[index]->position = {xForRank(resources[index]->layoutRank), -150.0f - (float)index * 104.0f};
	}

	ProcessFlowBounds ProcessFlowLayout::bounds(ProcessFlowModel const& model) const
	{
		ProcessFlowBounds result;
		for (auto const& node : model.nodes)
		{
			auto maximum = node.position + node.size;
			if (!result.valid) { result.minimum = node.position; result.maximum = maximum; result.valid = true; }
			else { result.minimum = glm::min(result.minimum, node.position); result.maximum = glm::max(result.maximum, maximum); }
		}
		return result;
	}

	ProcessFlowTransform ProcessFlowLayout::fitAll(ProcessFlowModel const& model, glm::vec2 viewport, float padding) const
	{
		auto value = bounds(model); ProcessFlowTransform result;
		if (!value.valid || viewport.x <= padding * 2 || viewport.y <= padding * 2) return result;
		auto size = glm::max(value.maximum - value.minimum, glm::vec2(1.0f));
		result.zoom = std::clamp(std::min((viewport.x - padding * 2) / size.x, (viewport.y - padding * 2) / size.y), 0.25f, 2.5f);
		result.pan = (viewport - size * result.zoom) * 0.5f - value.minimum * result.zoom;
		return result;
	}

	ProcessFlowTransform ProcessFlowLayout::zoomAroundCursor(ProcessFlowTransform value, glm::vec2 cursor, float newZoom)
	{
		newZoom = std::clamp(newZoom, 0.25f, 2.5f);
		auto canvasPoint = (cursor - value.pan) / value.zoom;
		value.zoom = newZoom; value.pan = cursor - canvasPoint * value.zoom;
		return value;
	}

	void runProcessFlowLayoutTests()
	{
		ProcessFlowModel model;
		for (uint64_t index = 0; index < 4; ++index)
		{
			ProcessFlowNode node; node.id = index + 1; node.semanticKey = std::to_string(index); node.mainSpine = true; node.sequence = index + 1; node.layoutRank = (float)index;
			model.nodes.push_back(node);
		}
		ProcessFlowNode resource; resource.id = 9; resource.semanticKey = "resource"; resource.resourceCategory = ProcessFlowResourceCategory::AuthoredImages; resource.layoutRank = 1.5f; model.nodes.push_back(resource);
		ProcessFlowLayout layout; layout.apply(model); auto first = model.nodes;
		for (size_t index = 1; index < 4; ++index)
			if (model.nodes[index].position.x <= model.nodes[index - 1].position.x + model.nodes[index - 1].size.x)
				throw std::runtime_error("Process-flow main layout is not strictly increasing/non-overlapping.");
		layout.apply(model);
		for (size_t index = 0; index < model.nodes.size(); ++index)
			if (model.nodes[index].position != first[index].position) throw std::runtime_error("Process-flow layout is not deterministic.");
		for (size_t left = 0; left < model.nodes.size(); ++left)
			for (size_t right = left + 1; right < model.nodes.size(); ++right)
			{
				auto const& a = model.nodes[left]; auto const& b = model.nodes[right];
				bool overlap = a.position.x < b.position.x + b.size.x && a.position.x + a.size.x > b.position.x &&
				               a.position.y < b.position.y + b.size.y && a.position.y + a.size.y > b.position.y;
				if (overlap) throw std::runtime_error("Process-flow layout contains overlapping nodes.");
			}
		auto fit = layout.fitAll(model, {800, 500});
		if (fit.zoom < 0.25f || fit.zoom > 2.5f) throw std::runtime_error("Process-flow fit transform is invalid.");
		auto zoomed = ProcessFlowLayout::zoomAroundCursor(fit, {200, 100}, fit.zoom * 1.2f);
		auto before = (glm::vec2(200, 100) - fit.pan) / fit.zoom, after = (glm::vec2(200, 100) - zoomed.pan) / zoomed.zoom;
		if (glm::length(before - after) > 0.01f) throw std::runtime_error("Process-flow cursor zoom is unstable.");
	}
}
