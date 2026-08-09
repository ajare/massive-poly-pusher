#include "ProcessFlowView.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "imgui/imgui.h"

namespace pipeline_editor
{
	namespace
	{
		bool isBatchNode(ProcessFlowNode const& node)
		{
			return node.kind == ProcessFlowNodeKind::BatchSubmission || node.kind == ProcessFlowNodeKind::BatchGroup;
		}

		ImU32 nodeColour(ProcessFlowNode const& node)
		{
			ImVec4 colour;
			switch (node.kind)
			{
			case ProcessFlowNodeKind::AuthoredPass: colour = {0.16f, 0.38f, 0.62f, 1}; break;
			case ProcessFlowNodeKind::BatchSubmission:
			case ProcessFlowNodeKind::BatchGroup: colour = {0.23f, 0.48f, 0.31f, 1}; break;
			case ProcessFlowNodeKind::MsaaResolve: colour = {0.50f, 0.30f, 0.63f, 1}; break;
			case ProcessFlowNodeKind::Taa: colour = {0.50f, 0.25f, 0.52f, 1}; break;
			case ProcessFlowNodeKind::Ssaa: colour = {0.43f, 0.29f, 0.60f, 1}; break;
			case ProcessFlowNodeKind::Fxaa: colour = {0.35f, 0.27f, 0.58f, 1}; break;
			case ProcessFlowNodeKind::Presentation: colour = {0.58f, 0.34f, 0.18f, 1}; break;
			default: colour = {0.35f, 0.35f, 0.39f, 1}; break;
			}
			if (!node.enabled) { colour.x *= 0.48f; colour.y *= 0.48f; colour.z *= 0.48f; colour.w = 0.72f; }
			return ImGui::ColorConvertFloat4ToU32(colour);
		}
		ImU32 edgeColour(ProcessFlowEdgeKind kind)
		{
			switch (kind)
			{
			case ProcessFlowEdgeKind::Execution: return IM_COL32(205, 210, 220, 210);
			case ProcessFlowEdgeKind::Colour: return IM_COL32(80, 190, 255, 210);
			case ProcessFlowEdgeKind::Depth: return IM_COL32(245, 100, 100, 220);
			case ProcessFlowEdgeKind::Shadow: return IM_COL32(155, 100, 230, 220);
			case ProcessFlowEdgeKind::History: return IM_COL32(230, 100, 205, 220);
			case ProcessFlowEdgeKind::Import: return IM_COL32(80, 215, 150, 220);
			default: return IM_COL32(245, 170, 70, 220);
			}
		}
		bool intersects(ImVec2 a, ImVec2 b, ImVec2 minimum, ImVec2 maximum)
		{
			return std::max(a.x, b.x) >= minimum.x && std::min(a.x, b.x) <= maximum.x &&
			       std::max(a.y, b.y) >= minimum.y && std::min(a.y, b.y) <= maximum.y;
		}
	}

	bool ProcessFlowView::consumeFiltersChanged()
	{
		bool value = mFiltersChanged; mFiltersChanged = false; return value;
	}

	ProcessFlowSelection ProcessFlowView::draw(ProcessFlowModel& model, double sampleAgeSeconds,
	                                               ProcessFlowHighlight const& highlight)
	{
		ProcessFlowSelection selection;
		if (mExpandedPipelineGeneration != model.pipelineGeneration || mExpandedSceneGeneration != model.sceneGeneration)
		{
			mExpanded.clear();
			mExpandedPipelineGeneration = model.pipelineGeneration;
			mExpandedSceneGeneration = model.sceneGeneration;
		}
		bool expansionRestored = false;
		for (auto& node : model.nodes)
			if (isBatchNode(node) && node.expanded != mExpanded.contains(node.id))
			{ node.expanded = mExpanded.contains(node.id); expansionRestored = true; }
		if (expansionRestored) mLayout.apply(model);
		if (!ImGui::Begin("Process Flow")) { ImGui::End(); return selection; }
		if (ImGui::Button("Fit All")) mFitRequested = true;
		ImGui::SameLine(); if (ImGui::Button("Refresh")) mRefreshRequested = true;
		ImGui::SameLine();
		ImGui::TextDisabled(model.liveSample ? "Frame %llu | sample %.2fs old" : "Waiting for live batch sample...",
		                    (unsigned long long)model.frameSerial, sampleAgeSeconds);
		auto toggle = [&](char const* label, ProcessFlowResourceCategory category)
		{
			bool enabled = mFilters.visible(category); ImGui::SameLine();
			if (ImGui::Checkbox(label, &enabled))
			{
				if (enabled) mFilters.resources |= (uint32_t)category;
				else mFilters.resources &= ~(uint32_t)category;
				mFiltersChanged = true;
			}
		};
		ImGui::Separator();
		ImGui::TextDisabled("Resources:");
		toggle("Images", ProcessFlowResourceCategory::AuthoredImages);
		toggle("Imports", ProcessFlowResourceCategory::Imports);
		toggle("Outputs", ProcessFlowResourceCategory::NamedOutputs);
		toggle("MSAA", ProcessFlowResourceCategory::MsaaResources);
		toggle("TAA", ProcessFlowResourceCategory::TaaHistories);
		toggle("SSAA", ProcessFlowResourceCategory::SsaaTargets);
		toggle("FXAA", ProcessFlowResourceCategory::FxaaTargets);
		ImGui::NewLine(); ImGui::Checkbox("Execution edges", &mFilters.executionEdges);
		if (ImGui::IsItemEdited()) mFiltersChanged = true;
		ImGui::SameLine(); ImGui::Checkbox("Resource edges", &mFilters.resourceEdges);
		if (ImGui::IsItemEdited()) mFiltersChanged = true;
		ImGui::TextDisabled("Legend: grey execution | blue colour | red depth | purple history | orange output");
		if (!model.emptyState.empty())
		{
			ImGui::Separator();
			ImGui::TextDisabled("%s", model.emptyState.c_str());
			ImGui::End(); return selection;
		}
		if (!model.diagnostics.empty())
		{
			ImGui::Separator();
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.30f, 1.0f), "Process Flow unavailable");
			for (auto const& diagnostic : model.diagnostics) ImGui::BulletText("%s", diagnostic.c_str());
			ImGui::End(); return selection;
		}
		if (!model.warningBanner.empty())
			ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.18f, 1.0f), "%s", model.warningBanner.c_str());
		if (model.largeGraph)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.18f, 1.0f), "Large graph: all %zu nodes retained; navigation may be slower.", model.nodes.size());
		}
		auto available = ImGui::GetContentRegionAvail();
		available.x = std::max(available.x, 64.0f); available.y = std::max(available.y, 64.0f);
		bool fitted = mFitRequested || mFittedRevision == 0;
		if (fitted)
		{
			mTransform = mLayout.fitAll(model, {available.x, available.y});
			auto graphBounds = mLayout.bounds(model);
			if (graphBounds.valid && (graphBounds.maximum.y - graphBounds.minimum.y) * mTransform.zoom > available.y - 80.0f)
				mTransform.pan.y = 40.0f - graphBounds.minimum.y * mTransform.zoom;
			mFitRequested = false; mFittedRevision = model.revision;
		}
		ImGui::BeginChild("ProcessFlowScrollableCanvas", available, ImGuiChildFlags_Borders,
		                  ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		if (fitted) ImGui::SetScrollY(0.0f);
		auto innerAvailable = ImGui::GetContentRegionAvail();
		auto graphBounds = mLayout.bounds(model);
		float contentHeight = innerAvailable.y;
		if (graphBounds.valid)
			contentHeight = std::max(contentHeight,
			                         mTransform.pan.y + graphBounds.maximum.y * mTransform.zoom + 48.0f);
		ImGui::InvisibleButton("ProcessFlowCanvas", {std::max(innerAvailable.x, 64.0f), contentHeight},
		                       ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
		auto itemMinimum = ImGui::GetItemRectMin();
		auto childPosition = ImGui::GetWindowPos(), childSize = ImGui::GetWindowSize();
		ImVec2 canvasMinimum(childPosition.x + 1.0f, childPosition.y + 1.0f),
		       canvasMaximum(childPosition.x + childSize.x - ImGui::GetStyle().ScrollbarSize - 1.0f,
		                     childPosition.y + childSize.y - 1.0f);
		auto origin = glm::vec2(itemMinimum.x, itemMinimum.y);
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
		{
			mTransform.pan.x += ImGui::GetIO().MouseDelta.x;
			ImGui::SetScrollY(std::max(0.0f, ImGui::GetScrollY() - ImGui::GetIO().MouseDelta.y));
		}
		if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0.0f)
		{
			auto cursor = glm::vec2(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y) - origin;
			mTransform = ProcessFlowLayout::zoomAroundCursor(mTransform, cursor,
			                                                    mTransform.zoom * std::pow(1.14f, ImGui::GetIO().MouseWheel));
		}
		auto screen = [&](glm::vec2 point) { return origin + mTransform.pan + point * mTransform.zoom; };
		std::unordered_map<uint64_t, ProcessFlowNode*> nodeLookup;
		nodeLookup.reserve(model.nodes.size());
		for (auto& node : model.nodes) nodeLookup[node.id] = &node;
		auto selectedNode = [&](ProcessFlowNode const& node)
		{
			if (node.kind == ProcessFlowNodeKind::AuthoredPass && node.passId == highlight.pass) return true;
			if (node.kind == ProcessFlowNodeKind::Import && node.importIndex == highlight.import) return true;
			if (node.kind != ProcessFlowNodeKind::Import && node.imageId >= 0 && node.imageId == highlight.image) return true;
			if (!isBatchNode(node)) return false;
			if (!highlight.materialName.empty())
			{
				auto separator = highlight.materialName.rfind("::");
				auto leaf = separator == std::string::npos ? highlight.materialName : highlight.materialName.substr(separator + 2);
				if (node.materialName == highlight.materialName || node.materialName.ends_with("/" + highlight.materialName) ||
				    node.materialName.ends_with("/" + leaf)) return true;
			}
			return highlight.sceneObject >= 0 &&
			       std::find(node.sceneObjectIndices.begin(), node.sceneObjectIndices.end(), highlight.sceneObject) !=
			           node.sceneObjectIndices.end();
		};
		auto draw = ImGui::GetWindowDrawList(); draw->PushClipRect(canvasMinimum, canvasMaximum, true);
		uint64_t hoveredNode = 0;
		for (auto const& node : model.nodes)
		{
			auto minimum = screen(node.position), maximum = screen(node.position + node.size);
			ImVec2 a(minimum.x, minimum.y), b(maximum.x, maximum.y);
			if (ImGui::GetIO().MousePos.x >= a.x && ImGui::GetIO().MousePos.x <= b.x &&
			    ImGui::GetIO().MousePos.y >= a.y && ImGui::GetIO().MousePos.y <= b.y) hoveredNode = node.id;
		}
		for (auto const& edge : model.edges)
		{
			auto sourceIt = nodeLookup.find(edge.source), destinationIt = nodeLookup.find(edge.destination);
			if (sourceIt == nodeLookup.end() || destinationIt == nodeLookup.end()) continue;
			auto source = sourceIt->second, destination = destinationIt->second;
			auto start = screen(source->position + glm::vec2(source->size.x * 0.5f, source->size.y));
			auto end = screen(destination->position + glm::vec2(destination->size.x * 0.5f, 0.0f));
			if (!intersects({start.x, start.y}, {end.x, end.y}, canvasMinimum, canvasMaximum)) continue;
			auto colour = edgeColour(edge.kind); float width = hoveredNode == edge.source || hoveredNode == edge.destination ? 3.2f : 1.8f;
			float direction = end.y >= start.y ? 1.0f : -1.0f;
			float bend = std::max(35.0f, std::abs(end.y - start.y) * 0.38f);
			draw->AddBezierCubic({start.x, start.y}, {start.x, start.y + direction * bend},
			                     {end.x, end.y - direction * bend}, {end.x, end.y}, colour, width);
			ImVec2 tip(end.x, end.y), left(end.x - 5, end.y - direction * 8),
			       right(end.x + 5, end.y - direction * 8);
			draw->AddTriangleFilled(tip, left, right, colour);
			if (!edge.label.empty() && mTransform.zoom > 0.55f)
				draw->AddText({(start.x + end.x) * 0.5f + 7.0f, (start.y + end.y) * 0.5f}, colour, edge.label.c_str());
		}
		for (auto& node : model.nodes)
		{
			auto minimum = screen(node.position), maximum = screen(node.position + node.size);
			ImVec2 a(minimum.x, minimum.y), b(maximum.x, maximum.y);
			if (!intersects(a, b, canvasMinimum, canvasMaximum)) continue;
			auto fill = nodeColour(node);
			bool selected = selectedNode(node);
			auto border = node.id == hoveredNode ? IM_COL32(255, 240, 150, 255)
			                                  : selected ? IM_COL32(80, 225, 255, 255) : IM_COL32(90, 95, 110, 255);
			draw->AddRectFilled(a, b, fill, 7.0f);
			draw->AddRect(a, b, border, 7.0f, 0, node.id == hoveredNode || selected ? 2.5f : 1.2f);
			draw->PushClipRect({a.x + 3.0f, a.y + 3.0f}, {b.x - 3.0f, b.y - 3.0f}, true);
			float z = mTransform.zoom, fontSize = std::max(6.0f, ImGui::GetFontSize() * z * 1.10f);
			for (size_t label = 0; label < node.renderDocLabels.size(); ++label)
			{
				auto const& summary = label < node.renderDocLabelSummaries.size()
				                          ? node.renderDocLabelSummaries[label] : node.renderDocLabels[label];
				draw->AddText(nullptr, fontSize, {a.x + 11 * z, a.y + (9 + 22 * (float)label) * z},
				              IM_COL32(255, 190, 70, 255), summary.c_str());
			}
			if (mTransform.zoom >= 0.40f)
			{
				float bodyY = 9.0f + 22.0f * (float)node.renderDocLabels.size();
				draw->AddText(nullptr, fontSize, {a.x + 11 * z, a.y + bodyY * z}, IM_COL32_WHITE, node.title.c_str());
				draw->AddText(nullptr, fontSize, {a.x + 11 * z, a.y + (bodyY + 23) * z}, IM_COL32(215, 220, 230, 255), node.subtitle.c_str());
				if (node.orderWarning) draw->AddText(nullptr, fontSize, {b.x - 22 * z, a.y + bodyY * z}, IM_COL32(255, 190, 55, 255), "!");
				if (!node.enabled) draw->AddText(nullptr, fontSize, {a.x + 11 * z, b.y - 22 * z}, IM_COL32(245, 180, 120, 255), "bypassed");
				else if (!node.details.empty()) draw->AddText(nullptr, fontSize, {a.x + 11 * z, b.y - 22 * z}, IM_COL32(185, 195, 205, 255), node.details.c_str());
				if (node.expanded && isBatchNode(node))
					for (size_t index = 0; index < node.sceneObjectNames.size(); ++index)
					{
						float objectY = 67.0f + 22.0f * (float)node.renderDocLabels.size() + (float)index * 24.0f;
						draw->AddText(nullptr, fontSize, {a.x + 17 * z, a.y + objectY * z},
						              node.sceneObjectIndices[index] == highlight.sceneObject ? IM_COL32(90, 235, 255, 255)
						                                                                       : IM_COL32(210, 230, 210, 255),
						              node.sceneObjectNames[index].c_str());
					}
			}
			draw->PopClipRect();
		}
		draw->PopClipRect();
		if (hoveredNode)
		{
			auto* node = nodeLookup[hoveredNode];
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && isBatchNode(*node))
			{
				node->expanded = !node->expanded;
				if (node->expanded) mExpanded.insert(node->id); else mExpanded.erase(node->id);
				mLayout.apply(model);
			}
			else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				auto localY = (ImGui::GetIO().MousePos.y - (origin.y + mTransform.pan.y)) / mTransform.zoom - node->position.y;
				float objectStart = 64.0f + 22.0f * (float)node->renderDocLabels.size();
				if (node->expanded && localY >= objectStart && !node->sceneObjectIndices.empty())
				{
					auto object = std::min((size_t)((localY - objectStart) / 24.0f), node->sceneObjectIndices.size() - 1);
					selection.kind = ProcessFlowSelection::Kind::SceneObject;
					selection.sceneObjectIndex = node->sceneObjectIndices[object];
				}
				else if (isBatchNode(*node) && !node->materialName.empty())
				{ selection.kind = ProcessFlowSelection::Kind::Material; selection.materialName = node->materialName; }
				else if (node->passId >= 0) { selection.kind = ProcessFlowSelection::Kind::Pass; selection.index = node->passId; }
				else if (node->kind == ProcessFlowNodeKind::Import && node->importIndex >= 0)
				{ selection.kind = ProcessFlowSelection::Kind::Import; selection.index = node->importIndex; }
				else if (node->imageId >= 0) { selection.kind = ProcessFlowSelection::Kind::Image; selection.index = node->imageId; }
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip(); ImGui::TextUnformatted(node->title.c_str());
				for (auto const& label : node->renderDocLabels) ImGui::Text("RenderDoc: %s", label.c_str());
				if (!node->details.empty()) ImGui::TextUnformatted(node->details.c_str());
				if (node->orderWarning) ImGui::Text("Authored position %d; executed position %d after dependency compilation.", node->authoredPosition + 1, node->actualPosition + 1);
				if (!node->bypassReason.empty()) ImGui::TextWrapped("%s", node->bypassReason.c_str());
				ImGui::EndTooltip();
			}
		}
		ImGui::EndChild();
		ImGui::End(); return selection;
	}
}
