#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <mpp/ParticleEffectSpecification.h>

namespace particle_editor
{
	enum class SpatialTargetKind { EmitterTemplate, ChildEffect };

	struct SpatialTarget
	{
		SpatialTargetKind kind{ SpatialTargetKind::EmitterTemplate };
		size_t index{};
		bool operator==(SpatialTarget const&) const = default;
	};

	struct TransformComponents
	{
		glm::vec3 translation{};
		glm::vec3 rotationDegrees{};
		glm::vec3 scale{ 1.0f };
	};

	struct ViewportOverlay
	{
		SpatialTarget target;
		std::vector<glm::vec2> points;
		bool closed{ false };
		bool selected{ false };
	};

	glm::mat4 emitterTransform(mpp::ParticleEffectSpecification::EmitterTemplate const& emitter);
	void setEmitterTransform(mpp::ParticleEffectSpecification::EmitterTemplate& emitter, glm::mat4 const& transform);
	bool decomposeTransform(glm::mat4 const& transform, TransformComponents& components);
	glm::mat4 composeTransform(TransformComponents const& components);

	std::vector<ViewportOverlay> makeViewportOverlays(mpp::ParticleEffectSpecification const& specification,
		glm::mat4 const& viewProjection, glm::vec2 viewportSize, std::optional<SpatialTarget> selected = std::nullopt);
	std::optional<SpatialTarget> pickViewportOverlay(std::vector<ViewportOverlay> const& overlays,
		glm::vec2 viewportPoint, float tolerance = 9.0f);

	bool runParticleSpatialEditingTests(std::string* failure = nullptr);
}
