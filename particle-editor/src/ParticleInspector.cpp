#include "ParticleInspector.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <functional>
#include <string>
#include <tuple>
#include <utility>

#include <imgui/imgui.h>

#include "ParticleDocument.h"
#include "ParticleResourceLibrary.h"

namespace particle_editor
{
	namespace
	{
		template<typename Value, typename Draw, typename Apply>
		bool editValue(ParticleDocument& document, char const* commandName, Value const& current,
			Draw&& draw, Apply&& apply, ParticlePreviewChange change = ParticlePreviewChange::Live,
			bool coalesce = true)
		{
			auto value = current;
			bool const changed = draw(value);
			bool const ended = ImGui::IsItemDeactivatedAfterEdit();
			if (changed)
				document.executeEdit(commandName,
					[apply = std::forward<Apply>(apply), value = std::move(value)](auto& effect) mutable
						{ apply(effect, value); }, coalesce, change);
			if (ended) document.endContinuousEdit();
			return changed;
		}

		bool textValue(ParticleDocument& document, char const* label, char const* commandName,
			std::string const& current, std::function<void(std::string)> apply)
		{
			std::array<char, 512> buffer{};
			std::copy_n(current.data(), std::min(current.size(), buffer.size() - 1u), buffer.data());
			bool const changed = ImGui::InputText(label, buffer.data(), buffer.size());
			bool const ended = ImGui::IsItemDeactivatedAfterEdit();
			if (changed) apply(buffer.data());
			if (ended) document.endContinuousEdit();
			return changed;
		}

		bool resourceValue(ParticleDocument& document, ParticleResourceLibrary const& resources,
			ParticleResourceKind kind, char const* label, std::string const& current,
			std::function<void(std::string)> apply)
		{
			ImGui::PushID(label);
			std::array<char, 512> buffer{};
			std::copy_n(current.data(), std::min(current.size(), buffer.size() - 1u), buffer.data());
			ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - 34.0f));
			bool changed = ImGui::InputText("##LogicalName", buffer.data(), buffer.size());
			bool const ended = ImGui::IsItemDeactivatedAfterEdit();
			if (changed) apply(buffer.data());
			if (ended) document.endContinuousEdit();
			ImGui::SameLine();
			if (ImGui::BeginCombo("##AvailableResources", "...", ImGuiComboFlags_NoPreview))
			{
				if (ImGui::Selectable("<none>", current.empty()))
				{
					document.endContinuousEdit();
					apply({});
					document.endContinuousEdit();
					changed = true;
				}
				for (auto const& name : resources.names(kind))
					if (ImGui::Selectable(name.c_str(), current == name))
					{
						document.endContinuousEdit();
						apply(name);
						document.endContinuousEdit();
						changed = true;
					}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			ImGui::TextUnformatted(label);
			if (!current.empty() && !resources.resolves(current, kind))
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "(unresolved or wrong type)");
			}
			ImGui::PopID();
			return changed;
		}

		std::array<float, 4> usefulShapeParameters(uint32_t shape, std::array<float, 4> parameters)
		{
			if (shape == uint32_t(mpp::ParticleSpawnShape::Line) &&
				parameters[0] == 0.0f && parameters[1] == 0.0f && parameters[2] == 0.0f)
				parameters[1] = 0.5f;
			else if (shape == uint32_t(mpp::ParticleSpawnShape::Box) &&
				parameters[0] == 0.0f && parameters[1] == 0.0f && parameters[2] == 0.0f)
				parameters = { 0.5f, 0.5f, 0.5f, 0.0f };
			else if (shape >= uint32_t(mpp::ParticleSpawnShape::Sphere) && parameters[0] == 0.0f)
				parameters[0] = 0.5f;
			if (shape == uint32_t(mpp::ParticleSpawnShape::Cone) && parameters[1] == 0.0f)
				parameters[1] = 1.0f;
			return parameters;
		}

		std::array<float, 4>& fieldFrequencyStrength(mpp::EmitterSimData& simulation,
			mpp::ParticleBehaviourModule module)
		{
			if (module == mpp::ParticleBehaviourModule::Noise) return simulation.noiseFrequencyStrength;
			if (module == mpp::ParticleBehaviourModule::CurlNoise) return simulation.curlNoiseFrequencyStrength;
			if (module == mpp::ParticleBehaviourModule::Turbulence) return simulation.turbulenceFrequencyStrength;
			return simulation.vectorFieldFrequencyStrength;
		}

		std::array<float, 4>& fieldScrollTimeScale(mpp::EmitterSimData& simulation,
			mpp::ParticleBehaviourModule module)
		{
			if (module == mpp::ParticleBehaviourModule::Noise) return simulation.noiseScrollAndTimeScale;
			if (module == mpp::ParticleBehaviourModule::CurlNoise) return simulation.curlNoiseScrollAndTimeScale;
			if (module == mpp::ParticleBehaviourModule::Turbulence) return simulation.turbulenceScrollAndTimeScale;
			return simulation.vectorFieldScrollAndTimeScale;
		}

		bool drawFieldControls(ParticleDocument& document, size_t emitterIndex,
			mpp::ParticleBehaviourModule module, char const* name,
			std::array<float, 4> const& frequencyStrength, std::array<float, 4> const& scrollTimeScale)
		{
			ImGui::PushID(name);
			std::array<float, 3> frequency{ frequencyStrength[0], frequencyStrength[1], frequencyStrength[2] };
			if (editValue(document, "Change field frequency", frequency,
				[](auto& value) { return ImGui::DragFloat3("Frequency", value.data(), 0.01f); },
				[emitterIndex, module](auto& value, auto const& edited)
				{
					auto& target = fieldFrequencyStrength(value.emitterTemplates[emitterIndex].value.simulation, module);
					std::copy(edited.begin(), edited.end(), target.begin());
				})) { ImGui::PopID(); return true; }
			float strength = frequencyStrength[3];
			if (editValue(document, "Change field strength", strength,
				[](float& value) { return ImGui::DragFloat("Strength", &value, 0.01f); },
				[emitterIndex, module](auto& value, float edited)
					{ fieldFrequencyStrength(value.emitterTemplates[emitterIndex].value.simulation, module)[3] = edited; }))
			{ ImGui::PopID(); return true; }
			std::array<float, 3> scroll{ scrollTimeScale[0], scrollTimeScale[1], scrollTimeScale[2] };
			if (editValue(document, "Change field scroll", scroll,
				[](auto& value) { return ImGui::DragFloat3("Scroll", value.data(), 0.01f); },
				[emitterIndex, module](auto& value, auto const& edited)
				{
					auto& target = fieldScrollTimeScale(value.emitterTemplates[emitterIndex].value.simulation, module);
					std::copy(edited.begin(), edited.end(), target.begin());
				})) { ImGui::PopID(); return true; }
			float timeScale = scrollTimeScale[3];
			if (editValue(document, "Change field time scale", timeScale,
				[](float& value) { return ImGui::DragFloat("Time scale", &value, 0.01f); },
				[emitterIndex, module](auto& value, float edited)
					{ fieldScrollTimeScale(value.emitterTemplates[emitterIndex].value.simulation, module)[3] = edited; }))
			{ ImGui::PopID(); return true; }
			ImGui::PopID();
			return false;
		}

		float sampleCurve(mpp::ParticleCurve const& curve, float time)
		{
			if (curve.keys.empty()) return curve.defaultValue;
			if (time <= curve.keys.front().time) return curve.keys.front().value;
			if (time >= curve.keys.back().time) return curve.keys.back().value;
			auto right = std::upper_bound(curve.keys.begin(), curve.keys.end(), time,
				[](float value, auto const& key) { return value < key.time; });
			auto const& left = *(right - 1);
			float const span = right->time - left.time;
			float const amount = span > 0.0f ? (time - left.time) / span : 1.0f;
			return left.value + (right->value - left.value) * amount;
		}

		std::array<float, 3> sampleGradient(mpp::ParticleGradient const& gradient, float time)
		{
			if (gradient.keys.empty()) return gradient.defaultColour;
			if (time <= gradient.keys.front().time) return gradient.keys.front().colour;
			if (time >= gradient.keys.back().time) return gradient.keys.back().colour;
			auto right = std::upper_bound(gradient.keys.begin(), gradient.keys.end(), time,
				[](float value, auto const& key) { return value < key.time; });
			auto const& left = *(right - 1);
			float const span = right->time - left.time;
			float const amount = span > 0.0f ? (time - left.time) / span : 1.0f;
			std::array<float, 3> result;
			for (size_t channel = 0; channel < result.size(); ++channel)
				result[channel] = left.colour[channel] + (right->colour[channel] - left.colour[channel]) * amount;
			return result;
		}

		bool drawScalarCurve(ParticleDocument& document, size_t emitterIndex,
			mpp::ParticleScalarCurve curveSlot, std::optional<size_t>& selectedKey)
		{
			auto const curveIndex = size_t(curveSlot);
			auto const& curve = document.specification().emitterTemplates[emitterIndex].value.curves[curveIndex];
			if (selectedKey && *selectedKey >= curve.keys.size()) selectedKey.reset();

			float minimum = curve.defaultValue;
			float maximum = curve.defaultValue;
			for (auto const& key : curve.keys)
			{
				if (!std::isfinite(key.value)) continue;
				minimum = std::min(minimum, key.value);
				maximum = std::max(maximum, key.value);
			}
			if (!std::isfinite(minimum) || !std::isfinite(maximum)) { minimum = 0.0f; maximum = 1.0f; }
			if (maximum - minimum < 0.001f) { minimum -= 0.5f; maximum += 0.5f; }
			else { float padding = (maximum - minimum) * 0.15f; minimum -= padding; maximum += padding; }

			float const width = std::max(180.0f, ImGui::GetContentRegionAvail().x);
			ImVec2 const origin = ImGui::GetCursorScreenPos();
			ImVec2 const size(width, 180.0f);
			ImGui::InvisibleButton("##ScalarCurveCanvas", size, ImGuiButtonFlags_MouseButtonLeft);
			auto* draw = ImGui::GetWindowDrawList();
			draw->AddRectFilled(origin, { origin.x + size.x, origin.y + size.y }, IM_COL32(24, 27, 32, 255));
			for (int line = 0; line <= 4; ++line)
			{
				float x = origin.x + size.x * float(line) / 4.0f;
				float y = origin.y + size.y * float(line) / 4.0f;
				draw->AddLine({ x, origin.y }, { x, origin.y + size.y }, IM_COL32(65, 68, 74, 255));
				draw->AddLine({ origin.x, y }, { origin.x + size.x, y }, IM_COL32(65, 68, 74, 255));
			}
			auto point = [&](float time, float value)
			{
				return ImVec2(origin.x + std::clamp(time, 0.0f, 1.0f) * size.x,
					origin.y + (maximum - value) / (maximum - minimum) * size.y);
			};
			ImVec2 previous = point(0.0f, sampleCurve(curve, 0.0f));
			for (auto const& key : curve.keys)
			{
				auto current = point(key.time, key.value);
				draw->AddLine(previous, current, IM_COL32(100, 190, 255, 255), 2.0f);
				previous = current;
			}
			draw->AddLine(previous, point(1.0f, sampleCurve(curve, 1.0f)), IM_COL32(100, 190, 255, 255), 2.0f);

			auto const mouse = ImGui::GetIO().MousePos;
			std::optional<size_t> hoveredKey;
			float nearest = 64.0f;
			for (size_t index = 0; index < curve.keys.size(); ++index)
			{
				auto position = point(curve.keys[index].time, curve.keys[index].value);
				float dx = mouse.x - position.x, dy = mouse.y - position.y;
				float distance = dx * dx + dy * dy;
				if (distance <= nearest) { nearest = distance; hoveredKey = index; }
				draw->AddCircleFilled(position, selectedKey == index ? 6.0f : 4.5f,
					selectedKey == index ? IM_COL32(255, 205, 70, 255) : IM_COL32(220, 230, 240, 255));
			}
			bool const clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
			if (clicked && hoveredKey) { document.endContinuousEdit(); selectedKey = hoveredKey; }
			else if (clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				float time = std::clamp((mouse.x - origin.x) / size.x, 0.0f, 1.0f);
				float value = maximum - (mouse.y - origin.y) / size.y * (maximum - minimum);
				selectedKey = document.addScalarCurveKey(emitterIndex, curveSlot, time, value);
				return true;
			}
			if (selectedKey && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			{
				float time = std::clamp((mouse.x - origin.x) / size.x, 0.0f, 1.0f);
				float value = maximum - (mouse.y - origin.y) / size.y * (maximum - minimum);
				document.editScalarCurveKey(emitterIndex, curveSlot, *selectedKey, time, value, true);
				return true;
			}
			if (ImGui::IsItemDeactivated()) document.endContinuousEdit();

			float defaultValue = curve.defaultValue;
			bool changed = ImGui::InputFloat("Default value", &defaultValue, 0.0f, 0.0f, "%.6g");
			bool ended = ImGui::IsItemDeactivatedAfterEdit();
			if (changed) { document.setScalarCurveDefault(emitterIndex, curveSlot, defaultValue, true); return true; }
			if (ended) document.endContinuousEdit();
			if (ImGui::Button("Add key"))
			{
				selectedKey = document.addScalarCurveKey(emitterIndex, curveSlot, 0.5f, sampleCurve(curve, 0.5f));
				return true;
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(!selectedKey);
			if (ImGui::Button("Remove selected key"))
			{
				document.removeScalarCurveKey(emitterIndex, curveSlot, *selectedKey);
				selectedKey.reset();
				ImGui::EndDisabled();
				return true;
			}
			ImGui::EndDisabled();
			if (selectedKey)
			{
				auto const& key = curve.keys[*selectedKey];
				std::array<float, 2> numeric{ key.time, key.value };
				changed = ImGui::InputFloat2("Selected time / value", numeric.data(), "%.6g");
				ended = ImGui::IsItemDeactivatedAfterEdit();
				if (changed)
				{
					document.editScalarCurveKey(emitterIndex, curveSlot, *selectedKey,
						numeric[0], numeric[1], true);
					return true;
				}
				if (ended) document.endContinuousEdit();
			}
			ImGui::TextDisabled("Double-click the graph to add a key; drag keys to edit time and value.");
			return false;
		}

		bool drawColourGradient(ParticleDocument& document, size_t emitterIndex,
			std::optional<size_t>& selectedKey)
		{
			auto const& gradient = document.specification().emitterTemplates[emitterIndex].value.colourGradient;
			if (selectedKey && *selectedKey >= gradient.keys.size()) selectedKey.reset();
			float const width = std::max(180.0f, ImGui::GetContentRegionAvail().x);
			ImVec2 const origin = ImGui::GetCursorScreenPos();
			ImVec2 const size(width, 56.0f);
			ImGui::InvisibleButton("##ColourGradientCanvas", size, ImGuiButtonFlags_MouseButtonLeft);
			auto* draw = ImGui::GetWindowDrawList();
			for (int segment = 0; segment < 64; ++segment)
			{
				float leftTime = float(segment) / 64.0f;
				float rightTime = float(segment + 1) / 64.0f;
				auto left = sampleGradient(gradient, leftTime);
				auto right = sampleGradient(gradient, rightTime);
				draw->AddRectFilledMultiColor(
					{ origin.x + leftTime * size.x, origin.y }, { origin.x + rightTime * size.x, origin.y + 38.0f },
					ImGui::ColorConvertFloat4ToU32({ left[0], left[1], left[2], 1.0f }),
					ImGui::ColorConvertFloat4ToU32({ right[0], right[1], right[2], 1.0f }),
					ImGui::ColorConvertFloat4ToU32({ right[0], right[1], right[2], 1.0f }),
					ImGui::ColorConvertFloat4ToU32({ left[0], left[1], left[2], 1.0f }));
			}
			draw->AddRect(origin, { origin.x + size.x, origin.y + 38.0f }, IM_COL32(210, 215, 220, 255));
			auto const mouse = ImGui::GetIO().MousePos;
			std::optional<size_t> hoveredKey;
			float nearest = 81.0f;
			for (size_t index = 0; index < gradient.keys.size(); ++index)
			{
				float x = origin.x + std::clamp(gradient.keys[index].time, 0.0f, 1.0f) * size.x;
				ImVec2 position{ x, origin.y + 45.0f };
				float dx = mouse.x - position.x, dy = mouse.y - position.y;
				float distance = dx * dx + dy * dy;
				if (distance <= nearest) { nearest = distance; hoveredKey = index; }
				auto const& colour = gradient.keys[index].colour;
				draw->AddCircleFilled(position, selectedKey == index ? 7.0f : 5.0f,
					ImGui::ColorConvertFloat4ToU32({ colour[0], colour[1], colour[2], 1.0f }));
				draw->AddCircle(position, selectedKey == index ? 7.0f : 5.0f,
					selectedKey == index ? IM_COL32(255, 205, 70, 255) : IM_COL32(235, 235, 235, 255), 0, 2.0f);
			}
			bool const clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
			if (clicked && hoveredKey) { document.endContinuousEdit(); selectedKey = hoveredKey; }
			else if (clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				float time = std::clamp((mouse.x - origin.x) / size.x, 0.0f, 1.0f);
				selectedKey = document.addColourGradientKey(emitterIndex, time, sampleGradient(gradient, time));
				return true;
			}
			if (selectedKey && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			{
				float time = std::clamp((mouse.x - origin.x) / size.x, 0.0f, 1.0f);
				document.editColourGradientKey(emitterIndex, *selectedKey, time,
					gradient.keys[*selectedKey].colour, true);
				return true;
			}
			if (ImGui::IsItemDeactivated()) document.endContinuousEdit();

			auto defaultColour = gradient.defaultColour;
			bool changed = ImGui::ColorEdit3("Default colour", defaultColour.data(), ImGuiColorEditFlags_Float);
			bool ended = ImGui::IsItemDeactivatedAfterEdit();
			if (changed) { document.setColourGradientDefault(emitterIndex, defaultColour, true); return true; }
			if (ended) document.endContinuousEdit();
			if (ImGui::Button("Add colour key"))
			{
				selectedKey = document.addColourGradientKey(emitterIndex, 0.5f, sampleGradient(gradient, 0.5f));
				return true;
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(!selectedKey);
			if (ImGui::Button("Remove selected colour key"))
			{
				document.removeColourGradientKey(emitterIndex, *selectedKey);
				selectedKey.reset();
				ImGui::EndDisabled();
				return true;
			}
			ImGui::EndDisabled();
			if (selectedKey)
			{
				auto const& key = gradient.keys[*selectedKey];
				float time = key.time;
				changed = ImGui::InputFloat("Selected normalized time", &time, 0.0f, 0.0f, "%.6f");
				ended = ImGui::IsItemDeactivatedAfterEdit();
				if (changed)
				{
					document.editColourGradientKey(emitterIndex, *selectedKey, time, key.colour, true);
					return true;
				}
				if (ended) document.endContinuousEdit();
				auto colour = key.colour;
				changed = ImGui::ColorEdit3("Selected colour", colour.data(), ImGuiColorEditFlags_Float);
				ended = ImGui::IsItemDeactivatedAfterEdit();
				if (changed)
				{
					document.editColourGradientKey(emitterIndex, *selectedKey, key.time, colour, true);
					return true;
				}
				if (ended) document.endContinuousEdit();
			}
			ImGui::TextDisabled("Double-click the gradient to add a key; drag keys to edit normalized time.");
			return false;
		}
	}

	void ParticleInspector::draw(ParticleDocument& document, ParticleResourceLibrary const& resources)
	{
		if (!ImGui::Begin("Particle Effect"))
		{
			ImGui::End();
			return;
		}

		auto const& effect = document.specification();
		if (textValue(document, "Name", "Rename particle effect", effect.name,
			[&](std::string name)
			{
				document.executeEdit("Rename particle effect", [name = std::move(name)](auto& value)
					{ value.name = name; }, true, ParticlePreviewChange::Live);
			}))
		{
			ImGui::End();
			return;
		}
		ImGui::TextDisabled("Version %u", effect.version);
		ImGui::Text("Maximum particles: %u", effect.maximumParticleCount);
		ImGui::SameLine();
		ImGui::TextDisabled("(derived from emitter-template budgets)");
		if (effect.bounds)
		{
			ImGui::Text("Bounds center: %.2f, %.2f, %.2f", effect.bounds->center.x, effect.bounds->center.y,
				effect.bounds->center.z);
			ImGui::Text("Bounds size:   %.2f, %.2f, %.2f", effect.bounds->size.x, effect.bounds->size.y,
				effect.bounds->size.z);
		}
		else ImGui::TextDisabled("Bounds: unbounded");

		ImGui::SeparatorText("Emitter templates");
		for (size_t index = 0; index < effect.emitterTemplates.size(); ++index)
		{
			auto const& emitter = effect.emitterTemplates[index];
			bool const selected = document.hasSelectedEmitterTemplate() &&
				document.selectedEmitterTemplate() == index;
			if (ImGui::Selectable((emitter.name + "##Emitter" + std::to_string(index)).c_str(), selected))
				document.selectEmitterTemplate(index);
		}

		if (ImGui::Button("Add"))
		{
			document.addEmitterTemplate();
			ImGui::End();
			return;
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!document.hasSelectedEmitterTemplate());
		if (ImGui::Button("Duplicate"))
		{
			document.duplicateEmitterTemplate(document.selectedEmitterTemplate());
			ImGui::EndDisabled();
			ImGui::End();
			return;
		}
		ImGui::SameLine();
		bool const canMoveUp = document.hasSelectedEmitterTemplate() && document.selectedEmitterTemplate() > 0u;
		ImGui::BeginDisabled(!canMoveUp);
		if (ImGui::Button("Up"))
		{
			auto index = document.selectedEmitterTemplate();
			document.moveEmitterTemplate(index, index - 1u);
			ImGui::EndDisabled();
			ImGui::EndDisabled();
			ImGui::End();
			return;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		bool const canMoveDown = document.hasSelectedEmitterTemplate() &&
			document.selectedEmitterTemplate() + 1u < effect.emitterTemplates.size();
		ImGui::BeginDisabled(!canMoveDown);
		if (ImGui::Button("Down"))
		{
			auto index = document.selectedEmitterTemplate();
			document.moveEmitterTemplate(index, index + 1u);
			ImGui::EndDisabled();
			ImGui::EndDisabled();
			ImGui::End();
			return;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Remove"))
		{
			document.removeEmitterTemplate(document.selectedEmitterTemplate());
			ImGui::EndDisabled();
			ImGui::End();
			return;
		}
		ImGui::EndDisabled();

		if (!document.hasSelectedEmitterTemplate())
		{
			ImGui::TextDisabled("Add an emitter template to edit its spawn and appearance properties.");
			ImGui::End();
			return;
		}

		auto const emitterIndex = document.selectedEmitterTemplate();
		auto const& authored = effect.emitterTemplates[emitterIndex];
		auto const& simulation = authored.value.simulation;
		auto const& appearance = authored.value.appearance;
		auto const& lighting = authored.value.lighting;
		if (textValue(document, "Emitter name", "Rename emitter template", authored.name,
			[&](std::string name) { document.renameEmitterTemplate(emitterIndex, std::move(name), true); }))
		{
			ImGui::End();
			return;
		}

		static constexpr char const* shapes[]{ "Point", "Line", "Box", "Sphere", "Hemisphere", "Disc", "Cone" };
		static constexpr char const* emissionModes[]{ "Continuous", "Burst" };
		static constexpr char const* billboards[]{ "Camera facing", "Screen aligned", "Cylindrical", "Axis locked",
			"Velocity aligned", "Velocity stretched" };
		static constexpr char const* blends[]{ "Additive", "Alpha", "Weighted OIT" };

		if (ImGui::CollapsingHeader("Spawn", ImGuiTreeNodeFlags_DefaultOpen))
		{
			int shape = int(simulation.shapeSeedModulesBudget[0]);
			if (ImGui::Combo("Shape", &shape, shapes, int(std::size(shapes))))
			{
				document.executeEdit("Change spawn shape", [emitterIndex, shape](auto& value)
				{
					auto& spawn = value.emitterTemplates[emitterIndex].value.simulation;
					spawn.shapeSeedModulesBudget[0] = uint32_t(shape);
					spawn.shapeParameters = usefulShapeParameters(uint32_t(shape), spawn.shapeParameters);
				}, false, ParticlePreviewChange::Live);
				ImGui::End();
				return;
			}

			auto const shapeValue = simulation.shapeSeedModulesBudget[0];
			if (shapeValue == uint32_t(mpp::ParticleSpawnShape::Line) ||
				shapeValue == uint32_t(mpp::ParticleSpawnShape::Box))
			{
				auto parameters = simulation.shapeParameters;
				auto label = shapeValue == uint32_t(mpp::ParticleSpawnShape::Line) ? "Line half-vector" : "Box half-extents";
				if (editValue(document, "Change shape parameters", parameters,
					[&](auto& value) { return ImGui::DragFloat3(label, value.data(), 0.01f); },
					[emitterIndex](auto& value, auto const& edited)
						{ value.emitterTemplates[emitterIndex].value.simulation.shapeParameters = edited; }))
				{
					ImGui::End();
					return;
				}
			}
			else if (shapeValue >= uint32_t(mpp::ParticleSpawnShape::Sphere) &&
				shapeValue <= uint32_t(mpp::ParticleSpawnShape::Disc))
			{
				float radius = simulation.shapeParameters[0];
				if (editValue(document, "Change shape radius", radius,
					[](float& value) { return ImGui::DragFloat("Radius", &value, 0.01f, 0.0f, 100000.0f); },
					[emitterIndex](auto& value, float edited)
						{ value.emitterTemplates[emitterIndex].value.simulation.shapeParameters[0] = edited; }))
				{
					ImGui::End();
					return;
				}
			}
			else if (shapeValue == uint32_t(mpp::ParticleSpawnShape::Cone))
			{
				std::array<float, 2> cone{ simulation.shapeParameters[0], simulation.shapeParameters[1] };
				if (editValue(document, "Change cone shape", cone,
					[](auto& value) { return ImGui::DragFloat2("Radius / height", value.data(), 0.01f, 0.0f, 100000.0f); },
					[emitterIndex](auto& value, auto const& edited)
					{
						value.emitterTemplates[emitterIndex].value.simulation.shapeParameters[0] = edited[0];
						value.emitterTemplates[emitterIndex].value.simulation.shapeParameters[1] = edited[1];
					}))
				{
					ImGui::End();
					return;
				}
			}

			uint32_t seed = simulation.shapeSeedModulesBudget[1];
			if (editValue(document, "Change emitter seed", seed,
				[](uint32_t& value) { return ImGui::InputScalar("Deterministic seed", ImGuiDataType_U32, &value); },
				[emitterIndex](auto& value, uint32_t edited)
					{ value.emitterTemplates[emitterIndex].value.simulation.shapeSeedModulesBudget[1] = edited; }))
			{
				ImGui::End();
				return;
			}

			uint32_t budget = simulation.shapeSeedModulesBudget[3];
			if (editValue(document, "Change emitter budget", budget,
				[](uint32_t& value) { return ImGui::InputScalar("Particle budget", ImGuiDataType_U32, &value); },
				[emitterIndex](auto& value, uint32_t edited)
					{ value.emitterTemplates[emitterIndex].value.simulation.shapeSeedModulesBudget[3] = edited; },
				ParticlePreviewChange::Structural))
			{
				ImGui::End();
				return;
			}

			int emissionMode = int(simulation.emissionState[0]);
			if (ImGui::Combo("Emission", &emissionMode, emissionModes, int(std::size(emissionModes))))
			{
				document.executeEdit("Change emission mode", [emitterIndex, emissionMode](auto& value)
					{ value.emitterTemplates[emitterIndex].value.simulation.emissionState[0] = uint32_t(emissionMode); },
					false, ParticlePreviewChange::Structural);
				ImGui::End();
				return;
			}
			bool enabled = simulation.emissionState[1] != 0u;
			if (editValue(document, "Toggle emitter", enabled,
				[](bool& value) { return ImGui::Checkbox("Enabled", &value); },
				[emitterIndex](auto& value, bool edited)
					{ value.emitterTemplates[emitterIndex].value.simulation.emissionState[1] = edited ? 1u : 0u; },
				ParticlePreviewChange::Live, false))
			{
				ImGui::End();
				return;
			}
			if (simulation.emissionState[0] == 0u)
			{
				float rate = simulation.emissionRateAndPadding[0];
				if (editValue(document, "Change continuous emission rate", rate,
					[](float& value) { return ImGui::DragFloat("Rate", &value, 0.1f, 0.0f, 100000.0f, "%.2f / s"); },
					[emitterIndex](auto& value, float edited)
						{ value.emitterTemplates[emitterIndex].value.simulation.emissionRateAndPadding[0] = edited; }))
				{
					ImGui::End();
					return;
				}
			}
			else
			{
				uint32_t count = simulation.emissionState[2];
				if (editValue(document, "Change burst count", count,
					[](uint32_t& value) { return ImGui::InputScalar("Burst count", ImGuiDataType_U32, &value); },
					[emitterIndex](auto& value, uint32_t edited)
						{ value.emitterTemplates[emitterIndex].value.simulation.emissionState[2] = edited; },
					ParticlePreviewChange::Structural))
				{
					ImGui::End();
					return;
				}
			}

			auto vectorMinimum = simulation.initialVelocityMin;
			if (editValue(document, "Change minimum velocity", vectorMinimum,
				[](auto& value) { return ImGui::DragFloat3("Velocity min", value.data(), 0.01f); },
				[emitterIndex](auto& value, auto const& edited)
					{ value.emitterTemplates[emitterIndex].value.simulation.initialVelocityMin = edited; }))
			{ ImGui::End(); return; }
			auto vectorMaximum = simulation.initialVelocityMax;
			if (editValue(document, "Change maximum velocity", vectorMaximum,
				[](auto& value) { return ImGui::DragFloat3("Velocity max", value.data(), 0.01f); },
				[emitterIndex](auto& value, auto const& edited)
					{ value.emitterTemplates[emitterIndex].value.simulation.initialVelocityMax = edited; }))
			{ ImGui::End(); return; }

			auto colourMinimum = simulation.colourMin;
			if (editValue(document, "Change minimum spawn colour", colourMinimum,
				[](auto& value) { return ImGui::ColorEdit4("Colour min", value.data(), ImGuiColorEditFlags_Float); },
				[emitterIndex](auto& value, auto const& edited)
					{ value.emitterTemplates[emitterIndex].value.simulation.colourMin = edited; }))
			{ ImGui::End(); return; }
			auto colourMaximum = simulation.colourMax;
			if (editValue(document, "Change maximum spawn colour", colourMaximum,
				[](auto& value) { return ImGui::ColorEdit4("Colour max", value.data(), ImGuiColorEditFlags_Float); },
				[emitterIndex](auto& value, auto const& edited)
					{ value.emitterTemplates[emitterIndex].value.simulation.colourMax = edited; }))
			{ ImGui::End(); return; }

			std::array<float, 2> lifetime{ simulation.lifetimeSizeRanges[0], simulation.lifetimeSizeRanges[1] };
			if (editValue(document, "Change lifetime range", lifetime,
				[](auto& value) { return ImGui::DragFloat2("Lifetime min / max", value.data(), 0.01f); },
				[emitterIndex](auto& value, auto const& edited)
				{
					value.emitterTemplates[emitterIndex].value.simulation.lifetimeSizeRanges[0] = edited[0];
					value.emitterTemplates[emitterIndex].value.simulation.lifetimeSizeRanges[1] = edited[1];
				}))
			{ ImGui::End(); return; }
			std::array<float, 2> size{ simulation.lifetimeSizeRanges[2], simulation.lifetimeSizeRanges[3] };
			if (editValue(document, "Change size range", size,
				[](auto& value) { return ImGui::DragFloat2("Size min / max", value.data(), 0.01f); },
				[emitterIndex](auto& value, auto const& edited)
				{
					value.emitterTemplates[emitterIndex].value.simulation.lifetimeSizeRanges[2] = edited[0];
					value.emitterTemplates[emitterIndex].value.simulation.lifetimeSizeRanges[3] = edited[1];
				}))
			{ ImGui::End(); return; }
			std::array<float, 2> rotation{ simulation.rotationRanges[0], simulation.rotationRanges[1] };
			if (editValue(document, "Change rotation range", rotation,
				[](auto& value) { return ImGui::DragFloat2("Rotation min / max", value.data(), 0.01f); },
				[emitterIndex](auto& value, auto const& edited)
				{
					value.emitterTemplates[emitterIndex].value.simulation.rotationRanges[0] = edited[0];
					value.emitterTemplates[emitterIndex].value.simulation.rotationRanges[1] = edited[1];
				}))
			{ ImGui::End(); return; }
			std::array<float, 2> angularVelocity{ simulation.rotationRanges[2], simulation.rotationRanges[3] };
			if (editValue(document, "Change angular-velocity range", angularVelocity,
				[](auto& value) { return ImGui::DragFloat2("Angular velocity min / max", value.data(), 0.01f); },
				[emitterIndex](auto& value, auto const& edited)
				{
					value.emitterTemplates[emitterIndex].value.simulation.rotationRanges[2] = edited[0];
					value.emitterTemplates[emitterIndex].value.simulation.rotationRanges[3] = edited[1];
				}))
			{ ImGui::End(); return; }
		}

		if (ImGui::CollapsingHeader("Behaviours", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto const modules = simulation.shapeSeedModulesBudget[2];
			auto moduleSection = [&](mpp::ParticleBehaviourModule module, char const* name)
			{
				ImGui::PushID(name);
				bool enabled = (modules & uint32_t(module)) != 0u;
				bool changed = ImGui::Checkbox("##Enabled", &enabled);
				ImGui::PopID();
				ImGui::SameLine();
				bool open = ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_DefaultOpen);
				return std::tuple{ enabled, changed, open };
			};
			auto setModule = [&](mpp::ParticleBehaviourModule module, bool enabled)
			{
				document.executeEdit(std::string(enabled ? "Enable " : "Disable ") +
					(module == mpp::ParticleBehaviourModule::CurlNoise ? "curl noise" :
					 module == mpp::ParticleBehaviourModule::VectorField ? "vector field" : "behaviour module"),
					[emitterIndex, module, enabled](auto& value)
					{
						auto& mask = value.emitterTemplates[emitterIndex].value.simulation.shapeSeedModulesBudget[2];
						if (enabled) mask |= uint32_t(module); else mask &= ~uint32_t(module);
					}, false, ParticlePreviewChange::Live);
			};

			{
				auto [enabled, toggled, open] = moduleSection(mpp::ParticleBehaviourModule::Gravity, "Gravity");
				if (toggled) { if (open) ImGui::TreePop(); setModule(mpp::ParticleBehaviourModule::Gravity, enabled); ImGui::End(); return; }
				if (open)
				{
					std::array<float, 3> acceleration{ simulation.gravityAndDrag[0], simulation.gravityAndDrag[1], simulation.gravityAndDrag[2] };
					if (editValue(document, "Change gravity acceleration", acceleration,
						[](auto& value) { return ImGui::DragFloat3("Acceleration", value.data(), 0.01f); },
						[emitterIndex](auto& value, auto const& edited)
						{
							auto& target = value.emitterTemplates[emitterIndex].value.simulation.gravityAndDrag;
							std::copy(edited.begin(), edited.end(), target.begin());
						})) { ImGui::TreePop(); ImGui::End(); return; }
					ImGui::TreePop();
				}
			}
			{
				auto [enabled, toggled, open] = moduleSection(mpp::ParticleBehaviourModule::Drag, "Drag");
				if (toggled) { if (open) ImGui::TreePop(); setModule(mpp::ParticleBehaviourModule::Drag, enabled); ImGui::End(); return; }
				if (open)
				{
					float coefficient = simulation.gravityAndDrag[3];
					if (editValue(document, "Change drag coefficient", coefficient,
						[](float& value) { return ImGui::DragFloat("Coefficient", &value, 0.01f, 0.0f, 100000.0f, "%.4g", ImGuiSliderFlags_AlwaysClamp); },
						[emitterIndex](auto& value, float edited)
							{ value.emitterTemplates[emitterIndex].value.simulation.gravityAndDrag[3] = edited; }))
					{ ImGui::TreePop(); ImGui::End(); return; }
					ImGui::TreePop();
				}
			}

			for (auto const& field : std::array{
				std::pair{ mpp::ParticleBehaviourModule::Noise, "Noise" },
				std::pair{ mpp::ParticleBehaviourModule::CurlNoise, "Curl noise" },
				std::pair{ mpp::ParticleBehaviourModule::Turbulence, "Turbulence" },
				std::pair{ mpp::ParticleBehaviourModule::VectorField, "Vector field" } })
			{
				auto [enabled, toggled, open] = moduleSection(field.first, field.second);
				if (toggled) { if (open) ImGui::TreePop(); setModule(field.first, enabled); ImGui::End(); return; }
				if (open)
				{
					auto const& frequency = field.first == mpp::ParticleBehaviourModule::Noise ? simulation.noiseFrequencyStrength :
						field.first == mpp::ParticleBehaviourModule::CurlNoise ? simulation.curlNoiseFrequencyStrength :
						field.first == mpp::ParticleBehaviourModule::Turbulence ? simulation.turbulenceFrequencyStrength :
						simulation.vectorFieldFrequencyStrength;
					auto const& scroll = field.first == mpp::ParticleBehaviourModule::Noise ? simulation.noiseScrollAndTimeScale :
						field.first == mpp::ParticleBehaviourModule::CurlNoise ? simulation.curlNoiseScrollAndTimeScale :
						field.first == mpp::ParticleBehaviourModule::Turbulence ? simulation.turbulenceScrollAndTimeScale :
						simulation.vectorFieldScrollAndTimeScale;
					if (drawFieldControls(document, emitterIndex, field.first, field.second, frequency, scroll))
					{ ImGui::TreePop(); ImGui::End(); return; }
					if (field.first == mpp::ParticleBehaviourModule::Turbulence)
					{
						int octaves = int(simulation.turbulenceOctavesLacunarityGain[0]);
						if (editValue(document, "Change turbulence octaves", octaves,
							[](int& value) { return ImGui::SliderInt("Octaves", &value, 1, 8); },
							[emitterIndex](auto& value, int edited)
								{ value.emitterTemplates[emitterIndex].value.simulation.turbulenceOctavesLacunarityGain[0] = float(edited); }))
						{ ImGui::TreePop(); ImGui::End(); return; }
						float lacunarity = simulation.turbulenceOctavesLacunarityGain[1];
						if (editValue(document, "Change turbulence lacunarity", lacunarity,
							[](float& value) { return ImGui::DragFloat("Lacunarity", &value, 0.01f, 1.0f, 1000.0f, "%.4g", ImGuiSliderFlags_AlwaysClamp); },
							[emitterIndex](auto& value, float edited)
								{ value.emitterTemplates[emitterIndex].value.simulation.turbulenceOctavesLacunarityGain[1] = edited; }))
						{ ImGui::TreePop(); ImGui::End(); return; }
						float gain = simulation.turbulenceOctavesLacunarityGain[2];
						if (editValue(document, "Change turbulence gain", gain,
							[](float& value) { return ImGui::SliderFloat("Gain", &value, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp); },
							[emitterIndex](auto& value, float edited)
								{ value.emitterTemplates[emitterIndex].value.simulation.turbulenceOctavesLacunarityGain[2] = edited; }))
						{ ImGui::TreePop(); ImGui::End(); return; }
					}
					if (field.first == mpp::ParticleBehaviourModule::VectorField)
						ImGui::TextDisabled("Uses the editor-only vector-field resource selected in the viewport.");
					ImGui::TreePop();
				}
			}

			{
				auto [enabled, toggled, open] = moduleSection(mpp::ParticleBehaviourModule::Collision, "Collision");
				if (toggled) { if (open) ImGui::TreePop(); setModule(mpp::ParticleBehaviourModule::Collision, enabled); ImGui::End(); return; }
				if (open)
				{
					auto const sourceMask = simulation.collisionConfiguration[0];
					for (auto const& source : std::array{
						std::pair{ mpp::ParticleCollisionSource::ScreenSpace, "Screen space" },
						std::pair{ mpp::ParticleCollisionSource::Analytical, "Analytical" },
						std::pair{ mpp::ParticleCollisionSource::SignedDistanceField, "Signed distance field" } })
					{
						bool selected = (sourceMask & uint32_t(source.first)) != 0u;
						bool const onlySource = selected && (sourceMask & ~uint32_t(source.first)) == 0u;
						ImGui::BeginDisabled(onlySource);
						bool sourceChanged = ImGui::Checkbox(source.second, &selected);
						ImGui::EndDisabled();
						if (source.first != mpp::ParticleCollisionSource::SignedDistanceField) ImGui::SameLine();
						if (sourceChanged)
						{
							document.executeEdit("Change collision sources", [emitterIndex, source = source.first, selected](auto& value)
							{
								auto& mask = value.emitterTemplates[emitterIndex].value.simulation.collisionConfiguration[0];
								if (selected) mask |= uint32_t(source); else mask &= ~uint32_t(source);
							}, false, ParticlePreviewChange::Live);
							ImGui::TreePop(); ImGui::End(); return;
						}
					}
					static constexpr char const* responses[]{ "Bounce", "Slide", "Stop", "Kill", "Spawn secondary effect" };
					int response = int(simulation.collisionConfiguration[1]);
					if (ImGui::Combo("Response", &response, responses, int(std::size(responses))))
					{
						document.executeEdit("Change collision response", [emitterIndex, response](auto& value)
							{ value.emitterTemplates[emitterIndex].value.simulation.collisionConfiguration[1] = uint32_t(response); },
							false, ParticlePreviewChange::Live);
						ImGui::TreePop(); ImGui::End(); return;
					}
					auto collisionValue = [&](size_t component, char const* label, float minimum, float maximum)
					{
						float value = simulation.collisionParameters[component];
						return editValue(document, "Change collision parameter", value,
							[label, minimum, maximum](float& edited)
								{ return ImGui::DragFloat(label, &edited, 0.01f, minimum, maximum, "%.4g", ImGuiSliderFlags_AlwaysClamp); },
							[emitterIndex, component](auto& effect, float edited)
								{ effect.emitterTemplates[emitterIndex].value.simulation.collisionParameters[component] = edited; });
					};
					if (collisionValue(0u, "Restitution", 0.0f, 1000.0f) ||
						collisionValue(1u, "Friction", 0.0f, 1.0f) ||
						collisionValue(2u, "Radius scale", 0.0f, 1000.0f) ||
						collisionValue(3u, "Screen-space thickness", 0.0f, 100000.0f))
					{ ImGui::TreePop(); ImGui::End(); return; }
					ImGui::TextDisabled("Collision inputs are preview-only settings in the viewport.");
					ImGui::TreePop();
				}
			}
		}

		if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (resourceValue(document, resources, ParticleResourceKind::Texture, "Texture resource",
				authored.albedoTexture, [&](std::string texture)
				{
					document.executeEdit("Change billboard texture", [emitterIndex, texture = std::move(texture)](auto& value)
						{ value.emitterTemplates[emitterIndex].albedoTexture = texture; }, true,
						ParticlePreviewChange::Structural);
				}))
			{ ImGui::End(); return; }
			ImGui::TextDisabled("Choose an MPP logical name; empty uses the renderer-owned white fallback.");

			std::array<float, 3> tint{ appearance.tintAndAlpha[0], appearance.tintAndAlpha[1], appearance.tintAndAlpha[2] };
			if (editValue(document, "Change billboard tint", tint,
				[](auto& value) { return ImGui::ColorEdit3("Tint", value.data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR); },
				[emitterIndex](auto& value, auto const& edited)
				{
					auto& target = value.emitterTemplates[emitterIndex].value.appearance.tintAndAlpha;
					std::copy(edited.begin(), edited.end(), target.begin());
				}))
			{ ImGui::End(); return; }
			float alpha = appearance.tintAndAlpha[3];
			if (editValue(document, "Change billboard alpha", alpha,
				[](float& value) { return ImGui::DragFloat("Alpha", &value, 0.01f, 0.0f, 100.0f); },
				[emitterIndex](auto& value, float edited)
					{ value.emitterTemplates[emitterIndex].value.appearance.tintAndAlpha[3] = edited; }))
			{ ImGui::End(); return; }
			float emissive = appearance.appearance[0];
			if (editValue(document, "Change emissive intensity", emissive,
				[](float& value) { return ImGui::DragFloat("Emissive intensity", &value, 0.01f, 0.0f, 100000.0f); },
				[emitterIndex](auto& value, float edited)
					{ value.emitterTemplates[emitterIndex].value.appearance.appearance[0] = edited; }))
			{ ImGui::End(); return; }
			float softFade = appearance.appearance[1];
			if (editValue(document, "Change soft-fade distance", softFade,
				[](float& value) { return ImGui::DragFloat("Soft-fade distance", &value, 0.01f, 0.0f, 100000.0f); },
				[emitterIndex](auto& value, float edited)
					{ value.emitterTemplates[emitterIndex].value.appearance.appearance[1] = edited; }))
			{ ImGui::End(); return; }

			std::array<uint32_t, 2> atlas{ appearance.textureAndAtlas[2], appearance.textureAndAtlas[3] };
			if (editValue(document, "Change atlas dimensions", atlas,
				[](auto& value) { return ImGui::InputScalarN("Atlas columns / rows", ImGuiDataType_U32, value.data(), 2); },
				[emitterIndex](auto& value, auto const& edited)
				{
					value.emitterTemplates[emitterIndex].value.appearance.textureAndAtlas[2] = edited[0];
					value.emitterTemplates[emitterIndex].value.appearance.textureAndAtlas[3] = edited[1];
				}))
			{ ImGui::End(); return; }
			uint32_t frameCount = appearance.modes[0];
			if (editValue(document, "Change atlas frame count", frameCount,
				[](uint32_t& value) { return ImGui::InputScalar("Frame count", ImGuiDataType_U32, &value); },
				[emitterIndex](auto& value, uint32_t edited)
					{ value.emitterTemplates[emitterIndex].value.appearance.modes[0] = edited; }))
			{ ImGui::End(); return; }
			static constexpr char const* playbackModes[]{ "None", "Frame over life", "Fixed rate" };
			int playback = int(appearance.modes[1] & mpp::ParticleTexturePlaybackMask);
			if (ImGui::Combo("Playback", &playback, playbackModes, int(std::size(playbackModes))))
			{
				document.executeEdit("Change atlas playback", [emitterIndex, playback](auto& value)
				{
					auto& mode = value.emitterTemplates[emitterIndex].value.appearance.modes[1];
					mode = (mode & mpp::ParticleTextureRandomStartBit) | uint32_t(playback);
				}, false, ParticlePreviewChange::Live);
				ImGui::End(); return;
			}
			bool randomStart = (appearance.modes[1] & mpp::ParticleTextureRandomStartBit) != 0u;
			if (editValue(document, "Toggle random atlas start", randomStart,
				[](bool& value) { return ImGui::Checkbox("Random start frame", &value); },
				[emitterIndex](auto& value, bool edited)
				{
					auto& mode = value.emitterTemplates[emitterIndex].value.appearance.modes[1];
					if (edited) mode |= mpp::ParticleTextureRandomStartBit;
					else mode &= ~mpp::ParticleTextureRandomStartBit;
				}, ParticlePreviewChange::Live, false))
			{ ImGui::End(); return; }
			float animationRate = appearance.appearance[2];
			if (editValue(document, "Change atlas playback rate", animationRate,
				[](float& value) { return ImGui::DragFloat("Playback rate", &value, 0.05f, 0.0f, 100000.0f, "%.3g frames / s"); },
				[emitterIndex](auto& value, float edited)
					{ value.emitterTemplates[emitterIndex].value.appearance.appearance[2] = edited; }))
			{ ImGui::End(); return; }

			float maximumDistance = appearance.culling[0];
			if (editValue(document, "Change maximum draw distance", maximumDistance,
				[](float& value) { return ImGui::DragFloat("Maximum draw distance", &value, 0.1f, 0.0f, 1000000.0f); },
				[emitterIndex](auto& value, float edited)
					{ value.emitterTemplates[emitterIndex].value.appearance.culling[0] = edited; }))
			{ ImGui::End(); return; }
			float projectedSize = appearance.culling[1];
			if (editValue(document, "Change minimum projected size", projectedSize,
				[](float& value) { return ImGui::DragFloat("Minimum projected diameter", &value, 0.05f, 0.0f, 100000.0f, "%.3g px"); },
				[emitterIndex](auto& value, float edited)
					{ value.emitterTemplates[emitterIndex].value.appearance.culling[1] = edited; }))
			{ ImGui::End(); return; }
			ImGui::TextDisabled("A zero culling threshold disables that test.");

			int billboard = int(appearance.modes[2]);
			if (ImGui::Combo("Billboard", &billboard, billboards, int(std::size(billboards))))
			{
				document.executeEdit("Change billboard mode", [emitterIndex, billboard](auto& value)
					{ value.emitterTemplates[emitterIndex].value.appearance.modes[2] = uint32_t(billboard); },
					false, ParticlePreviewChange::Live);
				ImGui::End(); return;
			}
			int blend = int(appearance.modes[3]);
			if (ImGui::Combo("Blend class", &blend, blends, int(std::size(blends))))
			{
				document.executeEdit("Change blend class", [emitterIndex, blend](auto& value)
					{ value.emitterTemplates[emitterIndex].value.appearance.modes[3] = uint32_t(blend); },
					false, ParticlePreviewChange::Structural);
				ImGui::End(); return;
			}
			bool depthSort = appearance.sorting[0] == uint32_t(mpp::ParticleSortMode::BackToFront);
			if (editValue(document, "Toggle depth sorting", depthSort,
				[](bool& value) { return ImGui::Checkbox("Back-to-front depth sort", &value); },
				[emitterIndex](auto& value, bool edited)
				{
					value.emitterTemplates[emitterIndex].value.appearance.sorting[0] = uint32_t(edited ?
						mpp::ParticleSortMode::BackToFront : mpp::ParticleSortMode::None);
				}, ParticlePreviewChange::Live, false))
			{ ImGui::End(); return; }
			if (appearance.modes[3] != uint32_t(mpp::ParticleBlendClass::Alpha))
				ImGui::TextDisabled("Depth sorting is applied only to the Alpha blend class.");

			bool distortion = appearance.sorting[2] != 0u;
			if (editValue(document, "Toggle particle distortion", distortion,
				[](bool& value) { return ImGui::Checkbox("Distortion output", &value); },
				[emitterIndex](auto& value, bool edited)
					{ value.emitterTemplates[emitterIndex].value.appearance.sorting[2] = edited ? 1u : 0u; },
				ParticlePreviewChange::Live, false))
			{ ImGui::End(); return; }
			float distortionStrength = appearance.culling[3];
			if (editValue(document, "Change distortion strength", distortionStrength,
				[](float& value) { return ImGui::DragFloat("Distortion strength", &value, 0.0005f, 0.0f, 10.0f, "%.5g UV"); },
				[emitterIndex](auto& value, float edited)
					{ value.emitterTemplates[emitterIndex].value.appearance.culling[3] = edited; }))
			{ ImGui::End(); return; }
			ImGui::TextDisabled("Distortion is billboard-only; zero strength disables its graph contribution.");
		}

		if (ImGui::CollapsingHeader("Mesh"))
		{
			if (resourceValue(document, resources, ParticleResourceKind::Model, "Model resource",
				authored.meshModel, [&](std::string model)
				{
					document.executeEdit("Change mesh-particle model", [emitterIndex, model = std::move(model)](auto& value)
					{
						auto& target = value.emitterTemplates[emitterIndex];
						target.meshModel = model;
						target.value.appearance.sorting[1] = uint32_t(model.empty() ?
							mpp::ParticleRenderMode::Billboard : mpp::ParticleRenderMode::Mesh);
						if (model.empty()) target.meshMaterial.clear();
					}, true, ParticlePreviewChange::Structural);
				}))
			{ ImGui::End(); return; }
			if (!authored.meshModel.empty())
			{
				if (resourceValue(document, resources, ParticleResourceKind::Material, "Material override",
					authored.meshMaterial, [&](std::string material)
					{
						document.executeEdit("Change mesh-particle material", [emitterIndex, material = std::move(material)](auto& value)
							{ value.emitterTemplates[emitterIndex].meshMaterial = material; }, true,
							ParticlePreviewChange::Structural);
					}))
				{ ImGui::End(); return; }
				ImGui::TextDisabled("Empty keeps each model mesh's embedded ordinary Material.");
			}
			else ImGui::TextDisabled("Select a Model to use the dedicated mesh-particle pass; empty renders a billboard.");
		}

		if (ImGui::CollapsingHeader("Lighting"))
		{
			auto const flags = lighting.flagsAndPadding[0];
			auto toggleLighting = [&](mpp::ParticleLightingFlag flag, char const* label, char const* command)
			{
				bool enabled = (flags & uint32_t(flag)) != 0u;
				if (!ImGui::Checkbox(label, &enabled)) return false;
				document.executeEdit(command, [emitterIndex, flag, enabled](auto& value)
				{
					auto& edited = value.emitterTemplates[emitterIndex].value.lighting.flagsAndPadding[0];
					if (enabled) edited |= uint32_t(flag); else edited &= ~uint32_t(flag);
					if (flag == mpp::ParticleLightingFlag::ProxyLight && !enabled)
						edited &= ~uint32_t(mpp::ParticleLightingFlag::PbrLightInjection);
				}, false, ParticlePreviewChange::Structural);
				return true;
			};
			if (toggleLighting(mpp::ParticleLightingFlag::ProxyLight, "Particle proxy light", "Toggle particle proxy light"))
			{ ImGui::End(); return; }
			bool const proxy = (flags & uint32_t(mpp::ParticleLightingFlag::ProxyLight)) != 0u;
			ImGui::BeginDisabled(!proxy);
			if (toggleLighting(mpp::ParticleLightingFlag::PbrLightInjection, "Inject into PBR lights", "Toggle PBR light injection"))
			{ ImGui::EndDisabled(); ImGui::End(); return; }
			ImGui::EndDisabled();
			if (!proxy) ImGui::TextDisabled("PBR light injection requires a particle proxy light.");
			if (toggleLighting(mpp::ParticleLightingFlag::VolumetricContribution,
				"Volumetric contribution", "Toggle particle volumetric contribution"))
			{ ImGui::End(); return; }

			bool const anyLighting = flags != 0u;
			ImGui::BeginDisabled(!anyLighting);
			std::array<float, 3> colour{ lighting.colourAndIntensity[0], lighting.colourAndIntensity[1],
				lighting.colourAndIntensity[2] };
			if (editValue(document, "Change particle lighting colour", colour,
				[](auto& value) { return ImGui::ColorEdit3("Radiance colour", value.data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR); },
				[emitterIndex](auto& value, auto const& edited)
				{
					auto& target = value.emitterTemplates[emitterIndex].value.lighting.colourAndIntensity;
					std::copy(edited.begin(), edited.end(), target.begin());
				}, ParticlePreviewChange::Structural))
			{ ImGui::EndDisabled(); ImGui::End(); return; }
			float intensity = lighting.colourAndIntensity[3];
			if (editValue(document, "Change particle lighting intensity", intensity,
				[](float& value) { return ImGui::DragFloat("Direct intensity", &value, 0.05f, 0.0f, 1000000.0f); },
				[emitterIndex](auto& value, float edited)
					{ value.emitterTemplates[emitterIndex].value.lighting.colourAndIntensity[3] = edited; },
				ParticlePreviewChange::Structural))
			{ ImGui::EndDisabled(); ImGui::End(); return; }
			float range = lighting.rangeAndVolumetric[0];
			if (editValue(document, "Change particle lighting range", range,
				[](float& value) { return ImGui::DragFloat("Range", &value, 0.05f, 0.0001f, 1000000.0f, "%.4g", ImGuiSliderFlags_AlwaysClamp); },
				[emitterIndex](auto& value, float edited)
					{ value.emitterTemplates[emitterIndex].value.lighting.rangeAndVolumetric[0] = edited; },
				ParticlePreviewChange::Structural))
			{ ImGui::EndDisabled(); ImGui::End(); return; }
			float volumetricIntensity = lighting.rangeAndVolumetric[1];
			if (editValue(document, "Change particle volumetric intensity", volumetricIntensity,
				[](float& value) { return ImGui::DragFloat("Volumetric intensity", &value, 0.01f, 0.0f, 1000000.0f); },
				[emitterIndex](auto& value, float edited)
					{ value.emitterTemplates[emitterIndex].value.lighting.rangeAndVolumetric[1] = edited; },
				ParticlePreviewChange::Structural))
			{ ImGui::EndDisabled(); ImGui::End(); return; }
			ImGui::EndDisabled();
			ImGui::TextDisabled("Lighting is bounded to at most one proxy light and one volume per live Emitter.");
		}

		if (ImGui::CollapsingHeader("Curves", ImGuiTreeNodeFlags_DefaultOpen))
		{
			static constexpr char const* curveNames[]{ "Size", "Alpha", "Velocity multiplier", "Drag",
				"Rotation speed", "Emissive intensity" };
			if (mEditedEmitter != emitterIndex)
			{
				document.endContinuousEdit();
				mEditedEmitter = emitterIndex;
				mSelectedScalarKey.reset();
				mSelectedGradientKey.reset();
			}
			int previousCurve = mSelectedScalarCurve;
			if (ImGui::Combo("Scalar curve", &mSelectedScalarCurve, curveNames, int(std::size(curveNames))))
			{
				if (previousCurve != mSelectedScalarCurve) document.endContinuousEdit();
				mSelectedScalarKey.reset();
			}
			if (drawScalarCurve(document, emitterIndex,
				mpp::ParticleScalarCurve(mSelectedScalarCurve), mSelectedScalarKey))
			{ ImGui::End(); return; }
			ImGui::SeparatorText("Colour over life");
			if (drawColourGradient(document, emitterIndex, mSelectedGradientKey))
			{ ImGui::End(); return; }
		}

		if (!effect.childEffects.empty())
		{
			ImGui::SeparatorText("Child particle effects");
			for (auto const& child : effect.childEffects)
				ImGui::BulletText("%s (seed %u)", child.effect.c_str(), child.seed);
		}
		ImGui::End();
	}
}
