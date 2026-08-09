#include "ProcessFlowLayout.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace pipeline_editor
{
	void ProcessFlowLayout::apply(ProcessFlowModel& model) const
	{
		std::vector<ProcessFlowNode*> spine, batches, disabled, resources;
		for (auto& node : model.nodes)
		{
			bool batch = node.kind == ProcessFlowNodeKind::BatchSubmission || node.kind == ProcessFlowNodeKind::BatchGroup;
			size_t labelCharacters = node.title.size();
			for (auto const& label : node.renderDocLabelSummaries) labelCharacters = std::max(labelCharacters, label.size());
			for (auto const& label : node.inputLabels) labelCharacters = std::max(labelCharacters, label.size());
			for (auto const& label : node.outputLabels) labelCharacters = std::max(labelCharacters, label.size());
			node.size.x = std::clamp(20.0f + 7.0f * (float)labelCharacters,
			                         batch ? 300.0f : 280.0f, 480.0f);
			float labelHeight = 36.0f * (float)node.renderDocLabels.size();
			float passIoHeight = node.kind == ProcessFlowNodeKind::AuthoredPass
			                         ? 20.0f * (float)(node.inputLabels.size() + node.outputLabels.size()) +
			                               (!node.inputLabels.empty() && !node.outputLabels.empty() ? 4.0f : 0.0f)
			                         : 0.0f;
			node.size.y = batch && node.expanded
			                  ? 84.0f + labelHeight + 22.0f * (float)std::max<size_t>(1, node.sceneObjectNames.size())
			                  : 80.0f + labelHeight + passIoHeight;
			if (node.mainSpine) spine.push_back(&node);
			else if ((batch || node.kind == ProcessFlowNodeKind::GlState) && node.parentPassId >= 0) batches.push_back(&node);
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
		std::unordered_map<int, std::vector<ProcessFlowNode*>> batchesByPass;
		for (auto* node : batches) batchesByPass[node->parentPassId].push_back(node);
		for (auto& [passId, children] : batchesByPass)
			std::stable_sort(children.begin(), children.end(), [](auto left, auto right) { return left->sequence < right->sequence; });
		float y = 0.0f;
		for (auto* node : spine)
		{
			node->position = {-node->size.x * 0.5f, y};
			auto children = node->kind == ProcessFlowNodeKind::AuthoredPass ? batchesByPass.find(node->passId) : batchesByPass.end();
			if (children == batchesByPass.end()) { y += node->size.y + 72.0f; continue; }
			float childWidth = 0.0f;
			for (auto const* child : children->second) childWidth = std::max(childWidth, child->size.x);
			float childY = node->position.y + (node->size.y - children->second.front()->size.y) * 0.5f;
			float childBottom = childY;
			for (auto* child : children->second)
			{
				child->position = {node->position.x - childWidth - 72.0f + (childWidth - child->size.x) * 0.5f, childY};
				childY += child->size.y + 48.0f;
				childBottom = childY - 48.0f;
			}
			y = std::max(node->position.y + node->size.y, childBottom) + 72.0f;
		}
		for (auto const& [passId, children] : batchesByPass)
			if (std::none_of(spine.begin(), spine.end(), [=](auto const* node)
			    { return node->kind == ProcessFlowNodeKind::AuthoredPass && node->passId == passId; }))
				disabled.insert(disabled.end(), children.begin(), children.end());
		auto yForRank = [&](float rank)
		{
			if (spine.empty()) return rank * 150.0f;
			auto nearest = *std::min_element(spine.begin(), spine.end(), [&](auto left, auto right)
			{ return std::abs(left->layoutRank - rank) < std::abs(right->layoutRank - rank); });
			return nearest->position.y + (rank > nearest->layoutRank ? nearest->size.y * 0.55f : 0.0f);
		};
		auto orderByRank = [](auto left, auto right)
		{
			if (left->layoutRank != right->layoutRank) return left->layoutRank < right->layoutRank;
			return left->semanticKey < right->semanticKey;
		};
		std::stable_sort(disabled.begin(), disabled.end(), orderByRank);
		float disabledBottom = -104.0f;
		for (auto* node : disabled)
		{
			auto nodeY = std::max(yForRank(node->layoutRank), disabledBottom + 26.0f);
			node->position = {900.0f - node->size.x * 0.5f, nodeY}; disabledBottom = nodeY + node->size.y;
		}
		std::stable_sort(resources.begin(), resources.end(), orderByRank);
		float resourceBottom = -104.0f, outputBottom = -104.0f;
		for (auto* node : resources)
		{
			bool output = node->resourceCategory == ProcessFlowResourceCategory::NamedOutputs;
			auto& bottom = output ? outputBottom : resourceBottom;
			auto nodeY = std::max(yForRank(node->layoutRank), bottom + 26.0f);
			node->position = {(output ? 1600.0f : -900.0f) - node->size.x * 0.5f, nodeY}; bottom = nodeY + node->size.y;
		}
		if (!model.nodes.empty())
		{
			float minimum = model.nodes.front().position.x, maximum = minimum + model.nodes.front().size.x;
			for (auto const& node : model.nodes)
			{
				minimum = std::min(minimum, node.position.x);
				maximum = std::max(maximum, node.position.x + node.size.x);
			}
			float offset = -(minimum + maximum) * 0.5f;
			for (auto& node : model.nodes) node.position.x += offset;
		}
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
		result.zoom = 1.0f;
		result.pan = {padding - value.minimum.x, (viewport.y - size.y) * 0.5f - value.minimum.y};
		return result;
	}

	ProcessFlowTransform ProcessFlowLayout::zoomAroundCursor(ProcessFlowTransform value, glm::vec2 cursor, float newZoom)
	{
		newZoom = 1.0f;
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
		for (size_t index = 0; index < 4; ++index) model.nodes[index].passId = (int)index;
		ProcessFlowNode batch; batch.id = 8; batch.semanticKey = "batch"; batch.kind = ProcessFlowNodeKind::BatchSubmission; batch.passId = 0; batch.parentPassId = 0; batch.sequence = 1; model.nodes.push_back(batch);
		ProcessFlowNode resource; resource.id = 9; resource.semanticKey = "resource"; resource.resourceCategory = ProcessFlowResourceCategory::AuthoredImages; resource.layoutRank = 1.5f; model.nodes.push_back(resource);
		ProcessFlowLayout layout; layout.apply(model); auto first = model.nodes;
		if (model.nodes[4].position.x + model.nodes[4].size.x >= model.nodes[0].position.x ||
		    std::abs((model.nodes[4].position.y + model.nodes[4].size.y * 0.5f) -
		             (model.nodes[0].position.y + model.nodes[0].size.y * 0.5f)) > 0.01f)
			throw std::runtime_error("Process-flow batch child is not left-aligned with its parent pass.");
		auto centredBounds = layout.bounds(model);
		if (std::abs(centredBounds.minimum.x + centredBounds.maximum.x) > 0.01f)
			throw std::runtime_error("Process-flow graph is not horizontally centred.");
		for (size_t index = 1; index < 4; ++index)
			if (model.nodes[index].position.y <= model.nodes[index - 1].position.y + model.nodes[index - 1].size.y)
				throw std::runtime_error("Process-flow vertical layout is not strictly increasing/non-overlapping.");
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
