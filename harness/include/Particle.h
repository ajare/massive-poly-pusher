#pragma once

#include <glm/vec3.hpp>

struct Particle
{
	glm::vec2 position;
	glm::vec4 colour;
	float distance;
	float angle;
};