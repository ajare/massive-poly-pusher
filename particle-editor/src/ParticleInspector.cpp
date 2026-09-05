#include "ParticleInspector.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include <imgui/imgui.h>

#include "ParticleDocument.h"

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
	}

	void ParticleInspector::draw(ParticleDocument& document)
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

		if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (textValue(document, "Texture resource", "Change billboard texture", authored.albedoTexture,
				[&](std::string texture)
				{
					document.executeEdit("Change billboard texture", [emitterIndex, texture = std::move(texture)](auto& value)
						{ value.emitterTemplates[emitterIndex].albedoTexture = texture; }, true,
						ParticlePreviewChange::Structural);
				}))
			{ ImGui::End(); return; }
			ImGui::TextDisabled("Use an MPP logical resource name; empty uses the white fallback.");

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

			int billboard = int(appearance.modes[2]);
			if (ImGui::Combo("Billboard", &billboard, billboards, int(std::size(billboards))))
			{
				document.executeEdit("Change billboard mode", [emitterIndex, billboard](auto& value)
					{ value.emitterTemplates[emitterIndex].value.appearance.modes[2] = uint32_t(billboard); },
					false, ParticlePreviewChange::Live);
				ImGui::End();
				return;
			}
			int blend = int(appearance.modes[3]);
			if (ImGui::Combo("Blend class", &blend, blends, int(std::size(blends))))
			{
				document.executeEdit("Change blend class", [emitterIndex, blend](auto& value)
					{ value.emitterTemplates[emitterIndex].value.appearance.modes[3] = uint32_t(blend); },
					false, ParticlePreviewChange::Structural);
				ImGui::End();
				return;
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
		}

		if (ImGui::CollapsingHeader("Curves"))
		{
			ImGui::Text("Size keys: %zu", authored.value.curves[size_t(mpp::ParticleScalarCurve::Size)].keys.size());
			ImGui::Text("Alpha keys: %zu", authored.value.curves[size_t(mpp::ParticleScalarCurve::Alpha)].keys.size());
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
