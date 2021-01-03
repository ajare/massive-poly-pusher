#pragma once

#include <vector>

#include "Trefoil.h"
#include "TrefoilWindow.h"

struct Vector3
{
	float px, py, pz;
	float nx, ny, nz;
	float r, g, b;

	Vector3(float x, float y, float z)
		: px(x)
		, py(y)
		, pz(z)
		, r(1.0f)
		, g(1.0f)
		, b(1.0f)
	{
	}
};

typedef std::vector<std::vector<Vector2>> VertexList;

Winding pointsWinding(std::vector<Vector2> const& points);

VertexList generateTrefoil(TrefoilWindow const* window, Trefoil const& trefoil, Vector2 const& position);

VertexList generateTrefoilPane_Classic(TrefoilWindow const* window, Trefoil const& trefoil, Vector2 const& position, float paneHeight);

VertexList generateTrefoilPane_HM(TrefoilWindow const* window, Trefoil const& trefoil, Vector2 const& position, float paneWidth, float paneHeight);

std::vector<Vector2> generateBorder(TrefoilWindow const* window, float x, float y, float width, float height, float shoulderHeight, float arcScale);

VertexList generateLines(TrefoilWindow const* window);

std::vector<Vector3> generateTriangles(TrefoilWindow const* window, VertexList const& lines);