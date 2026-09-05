#include "ParticleSpatialEditing.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace particle_editor
{
	namespace
	{
		constexpr float Pi = 3.14159265358979323846f;

		std::optional<glm::vec2> project(glm::mat4 const& viewProjection, glm::vec2 size, glm::vec3 point)
		{
			auto clip = viewProjection * glm::vec4(point, 1.0f);
			if (!std::isfinite(clip.w) || clip.w <= 0.00001f) return std::nullopt;
			auto ndc = glm::vec3(clip) / clip.w;
			if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y) || ndc.z < -1.0f || ndc.z > 1.0f) return std::nullopt;
			return glm::vec2((ndc.x * 0.5f + 0.5f) * size.x, (0.5f - ndc.y * 0.5f) * size.y);
		}

		void line(std::vector<ViewportOverlay>& result, SpatialTarget target, bool selected,
			glm::mat4 const& transform, glm::mat4 const& vp, glm::vec2 size, glm::vec3 a, glm::vec3 b)
		{
			auto pa = project(vp, size, glm::vec3(transform * glm::vec4(a, 1.0f)));
			auto pb = project(vp, size, glm::vec3(transform * glm::vec4(b, 1.0f)));
			if (pa && pb) result.push_back({ target, { *pa, *pb }, false, selected });
		}

		void loop(std::vector<ViewportOverlay>& result, SpatialTarget target, bool selected,
			glm::mat4 const& transform, glm::mat4 const& vp, glm::vec2 size,
			std::function<glm::vec3(float)> const& sample, float begin = 0.0f, float end = 2.0f * Pi, bool closed = true)
		{
			ViewportOverlay overlay{ target, {}, closed, selected };
			for (int i = 0; i <= 32; ++i)
			{
				float t = begin + (end - begin) * float(i) / 32.0f;
				auto point = project(vp, size, glm::vec3(transform * glm::vec4(sample(t), 1.0f)));
				if (point) overlay.points.push_back(*point);
			}
			if (overlay.points.size() > 1u) result.push_back(std::move(overlay));
		}

		float segmentDistanceSquared(glm::vec2 point, glm::vec2 a, glm::vec2 b)
		{
			auto edge = b - a;
			float denominator = glm::dot(edge, edge);
			float t = denominator > 0.0f ? glm::clamp(glm::dot(point - a, edge) / denominator, 0.0f, 1.0f) : 0.0f;
			auto difference = point - (a + edge * t);
			return glm::dot(difference, difference);
		}
	}

	glm::mat4 emitterTransform(mpp::ParticleEffectSpecification::EmitterTemplate const& emitter)
	{
		glm::mat4 result(1.0f);
		for (int column = 0; column < 4; ++column)
			for (int row = 0; row < 4; ++row)
				result[column][row] = emitter.value.simulation.transform[size_t(column * 4 + row)];
		return result;
	}

	void setEmitterTransform(mpp::ParticleEffectSpecification::EmitterTemplate& emitter, glm::mat4 const& transform)
	{
		emitter.value.localTransform = transform;
		for (int column = 0; column < 4; ++column)
			for (int row = 0; row < 4; ++row)
				emitter.value.simulation.transform[size_t(column * 4 + row)] = transform[column][row];
	}

	bool decomposeTransform(glm::mat4 const& transform, TransformComponents& components)
	{
		for (int column = 0; column < 4; ++column) for (int row = 0; row < 4; ++row)
			if (!std::isfinite(transform[column][row])) return false;
		if (std::abs(transform[0][3]) > 0.00001f || std::abs(transform[1][3]) > 0.00001f ||
			std::abs(transform[2][3]) > 0.00001f || std::abs(transform[3][3] - 1.0f) > 0.00001f) return false;
		glm::vec3 axes[]{ glm::vec3(transform[0]), glm::vec3(transform[1]), glm::vec3(transform[2]) };
		glm::vec3 scale{ glm::length(axes[0]), glm::length(axes[1]), glm::length(axes[2]) };
		if (std::min({ scale.x, scale.y, scale.z }) < 0.000001f) return false;
		for (int axis = 0; axis < 3; ++axis) axes[axis] /= scale[axis];
		if (std::abs(glm::dot(axes[0], axes[1])) > 0.0001f || std::abs(glm::dot(axes[0], axes[2])) > 0.0001f ||
			std::abs(glm::dot(axes[1], axes[2])) > 0.0001f) return false;
		if (glm::dot(glm::cross(axes[0], axes[1]), axes[2]) < 0.0f) { axes[0] = -axes[0]; scale.x = -scale.x; }
		glm::mat3 rotation(axes[0], axes[1], axes[2]);
		auto orientation = glm::normalize(glm::quat_cast(rotation));
		components = { glm::vec3(transform[3]), glm::degrees(glm::eulerAngles(orientation)), scale };
		auto recomposed = composeTransform(components);
		for (int column = 0; column < 4; ++column) for (int row = 0; row < 4; ++row)
			if (std::abs(recomposed[column][row] - transform[column][row]) > 0.0005f) return false;
		return true;
	}

	glm::mat4 composeTransform(TransformComponents const& components)
	{
		auto rotation = glm::mat4_cast(glm::quat(glm::radians(components.rotationDegrees)));
		return glm::translate(glm::mat4(1.0f), components.translation) * rotation *
			glm::scale(glm::mat4(1.0f), components.scale);
	}

	std::vector<ViewportOverlay> makeViewportOverlays(mpp::ParticleEffectSpecification const& specification,
		glm::mat4 const& vp, glm::vec2 size, std::optional<SpatialTarget> selected)
	{
		std::vector<ViewportOverlay> result;
		for (size_t index = 0; index < specification.emitterTemplates.size(); ++index)
		{
			auto const& emitter = specification.emitterTemplates[index];
			auto const& simulation = emitter.value.simulation;
			SpatialTarget target{ SpatialTargetKind::EmitterTemplate, index };
			bool isSelected = selected == target;
			auto transform = emitterTransform(emitter);
			auto const& p = simulation.shapeParameters;
			auto shape = mpp::ParticleSpawnShape(simulation.shapeSeedModulesBudget[0]);
			if (shape == mpp::ParticleSpawnShape::Point)
			{
				line(result, target, isSelected, transform, vp, size, {-.12f,0,0}, {.12f,0,0});
				line(result, target, isSelected, transform, vp, size, {0,-.12f,0}, {0,.12f,0});
				line(result, target, isSelected, transform, vp, size, {0,0,-.12f}, {0,0,.12f});
			}
			else if (shape == mpp::ParticleSpawnShape::Line) line(result, target, isSelected, transform, vp, size, {-p[0],-p[1],-p[2]}, {p[0],p[1],p[2]});
			else if (shape == mpp::ParticleSpawnShape::Box)
			{
				for (int axis = 0; axis < 3; ++axis) for (int a = -1; a <= 1; a += 2) for (int b = -1; b <= 1; b += 2)
				{
					glm::vec3 from(0.0f), to;
					from[(axis+1)%3]=a*p[(axis+1)%3]; from[(axis+2)%3]=b*p[(axis+2)%3];
					to=from; from[axis]=-p[axis]; to[axis]=p[axis]; line(result,target,isSelected,transform,vp,size,from,to);
				}
			}
			else
			{
				float radius = std::max(p[0], 0.01f);
				if (shape == mpp::ParticleSpawnShape::Cone)
				{
					loop(result,target,isSelected,transform,vp,size,[&](float a){return glm::vec3(std::cos(a)*radius,0,std::sin(a)*radius);});
					for (int i=0;i<4;++i){float a=float(i)*Pi*.5f;line(result,target,isSelected,transform,vp,size,{std::cos(a)*radius,0,std::sin(a)*radius},{0,p[1],0});}
				}
				else
				{
					loop(result,target,isSelected,transform,vp,size,[&](float a){return glm::vec3(std::cos(a)*radius,0,std::sin(a)*radius);});
					if (shape != mpp::ParticleSpawnShape::Disc)
					{
						float end = shape == mpp::ParticleSpawnShape::Hemisphere ? Pi : 2.0f*Pi;
						loop(result,target,isSelected,transform,vp,size,[&](float a){return glm::vec3(std::cos(a)*radius,std::sin(a)*radius,0);},0,end,shape != mpp::ParticleSpawnShape::Hemisphere);
						loop(result,target,isSelected,transform,vp,size,[&](float a){return glm::vec3(0,std::sin(a)*radius,std::cos(a)*radius);},0,end,shape != mpp::ParticleSpawnShape::Hemisphere);
					}
				}
			}
		}
		for (size_t index = 0; index < specification.childEffects.size(); ++index)
		{
			SpatialTarget target{ SpatialTargetKind::ChildEffect, index }; bool isSelected = selected == target;
			auto const& t = specification.childEffects[index].transform;
			line(result,target,isSelected,t,vp,size,{-.25f,0,0},{.25f,0,0}); line(result,target,isSelected,t,vp,size,{0,-.25f,0},{0,.25f,0});
			line(result,target,isSelected,t,vp,size,{0,0,-.25f},{0,0,.25f});
		}
		return result;
	}

	std::optional<SpatialTarget> pickViewportOverlay(std::vector<ViewportOverlay> const& overlays,
		glm::vec2 point, float tolerance)
	{
		float nearest = tolerance * tolerance; std::optional<SpatialTarget> result;
		for (auto const& overlay : overlays) for (size_t i = 1; i < overlay.points.size(); ++i)
		{
			float distance = segmentDistanceSquared(point, overlay.points[i-1], overlay.points[i]);
			if (distance <= nearest) { nearest = distance; result = overlay.target; }
		}
		return result;
	}

	bool runParticleSpatialEditingTests(std::string* failure)
	{
		auto fail=[&](char const* text){if(failure)*failure=text;return false;};
		TransformComponents expected{{1,2,3},{15,-25,35},{2,3,4}}, actual;
		auto matrix=composeTransform(expected); if(!decomposeTransform(matrix,actual)) return fail("TRS transform did not decompose");
		auto restored=composeTransform(actual); for(int c=0;c<4;++c)for(int r=0;r<4;++r)if(std::abs(matrix[c][r]-restored[c][r])>.001f)return fail("TRS transform did not round-trip");
		auto shear=glm::mat4(1);shear[1][0]=.3f;if(decomposeTransform(shear,actual))return fail("sheared matrix was not identified as advanced-only");
		mpp::ParticleEffectSpecification spec;spec.emitterTemplates.emplace_back();setEmitterTransform(spec.emitterTemplates[0],matrix);
		if(emitterTransform(spec.emitterTemplates[0])!=matrix||spec.emitterTemplates[0].value.localTransform!=matrix)return fail("emitter transform copies diverged");
		setEmitterTransform(spec.emitterTemplates[0], glm::mat4(1.0f));
		auto& simulation=spec.emitterTemplates[0].value.simulation; simulation.shapeParameters={.3f,.4f,.5f,0};
		for(uint32_t shape=uint32_t(mpp::ParticleSpawnShape::Point);shape<=uint32_t(mpp::ParticleSpawnShape::Cone);++shape)
		{
			simulation.shapeSeedModulesBudget[0]=shape;
			auto overlays=makeViewportOverlays(spec,glm::mat4(1),{200,200},SpatialTarget{SpatialTargetKind::EmitterTemplate,0});
			if(overlays.empty())return fail("a spawn shape produced no viewport overlay");
			auto const& segment=overlays.front().points;
			if(segment.size()<2u||pickViewportOverlay(overlays,(segment[0]+segment[1])*.5f)!=SpatialTarget{SpatialTargetKind::EmitterTemplate,0})
				return fail("viewport overlay picking did not select its emitter template");
		}
		spec.childEffects.emplace_back(); auto childOverlays=makeViewportOverlays(spec,glm::mat4(1),{200,200});
		if(std::none_of(childOverlays.begin(),childOverlays.end(),[](auto const& overlay){return overlay.target.kind==SpatialTargetKind::ChildEffect;}))
			return fail("child particle effect produced no viewport overlay");
		if(failure)failure->clear();return true;
	}
}
