#include "ParticleInspector.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include <imgui/imgui.h>

#include "ParticleDocument.h"

namespace particle_editor
{
	void ParticleInspector::draw(ParticleDocument& document)
	{
		if (!ImGui::Begin("Particle Effect"))
		{
			ImGui::End();
			return;
		}
		auto const& effect = document.specification();
		char effectName[256]{};
		std::copy_n(effect.name.data(), std::min(effect.name.size(), sizeof(effectName) - 1), effectName);
		if (ImGui::InputText("Name", effectName, sizeof(effectName)))
		{
			std::string name = effectName;
			document.executeEdit("Rename particle effect", [name = std::move(name)](auto& value)
				{ value.name = name; }, true);
			ImGui::End();
			return;
		}
		if (ImGui::IsItemDeactivatedAfterEdit()) document.endContinuousEdit();
		ImGui::TextDisabled("Version %u | %u particles", effect.version, effect.maximumParticleCount);
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
			if (ImGui::Selectable(emitter.name.c_str(), mSelectedEmitter == static_cast<int>(index)))
				mSelectedEmitter = static_cast<int>(index);
		}
		if (effect.emitterTemplates.empty()) mSelectedEmitter = -1;
		if (mSelectedEmitter >= static_cast<int>(effect.emitterTemplates.size())) mSelectedEmitter = 0;

		if (mSelectedEmitter >= 0)
		{
			auto const& authored = effect.emitterTemplates[size_t(mSelectedEmitter)];
			auto const& emitter = authored.value;
			auto const& simulation = emitter.simulation;
			auto const& appearance = emitter.appearance;
			static constexpr std::array shapes{ "Point", "Line", "Box", "Sphere", "Hemisphere", "Disc", "Cone" };
			static constexpr std::array blends{ "Additive", "Alpha", "Weighted OIT" };
			ImGui::SeparatorText("Inspector");
			if (ImGui::CollapsingHeader("Spawn", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto shape = simulation.shapeSeedModulesBudget[0];
				ImGui::Text("Shape: %s", shape < shapes.size() ? shapes[shape] : "Unknown");
				ImGui::Text("Deterministic seed: %u", simulation.shapeSeedModulesBudget[1]);
				ImGui::Text("Budget: %u", simulation.shapeSeedModulesBudget[3]);
				float emissionRate = simulation.emissionRateAndPadding[0];
				if (ImGui::DragFloat("Continuous rate", &emissionRate, 0.1f, 0.0f, 100000.0f, "%.2f / s"))
				{
					document.executeEdit("Change emission rate", [emissionRate, emitterIndex = size_t(mSelectedEmitter)](auto& value)
						{ value.emitterTemplates[emitterIndex].value.simulation.emissionRateAndPadding[0] = emissionRate; }, true);
					ImGui::End();
					return;
				}
				if (ImGui::IsItemDeactivatedAfterEdit()) document.endContinuousEdit();
				ImGui::Text("Lifetime: %.2f - %.2f s", simulation.lifetimeSizeRanges[0],
					simulation.lifetimeSizeRanges[1]);
				ImGui::Text("Size: %.2f - %.2f", simulation.lifetimeSizeRanges[2], simulation.lifetimeSizeRanges[3]);
			}
			if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto blend = appearance.modes[3];
				ImGui::Text("Blend class: %s", blend < blends.size() ? blends[blend] : "Unknown");
				ImGui::Text("Texture: %s", authored.albedoTexture.empty() ? "Dependency-free" : authored.albedoTexture.c_str());
				ImGui::Text("Emissive intensity: %.2f", appearance.appearance[0]);
			}
			if (ImGui::CollapsingHeader("Curves"))
			{
				ImGui::Text("Size keys: %zu", emitter.curves[size_t(mpp::ParticleScalarCurve::Size)].keys.size());
				ImGui::Text("Alpha keys: %zu", emitter.curves[size_t(mpp::ParticleScalarCurve::Alpha)].keys.size());
			}
		}
		if (!effect.childEffects.empty())
		{
			ImGui::SeparatorText("Child particle effects");
			for (auto const& child : effect.childEffects) ImGui::BulletText("%s (seed %u)", child.effect.c_str(), child.seed);
		}
		ImGui::End();
	}
}
