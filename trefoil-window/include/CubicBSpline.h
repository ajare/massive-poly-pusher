#pragma once

#include <vector>

#pragma warning(push)
#pragma warning(disable: 4244)
#include <spline_library/splines/uniform_cubic_bspline.h>
#pragma warning(pop)

#include "Vector2.h"
#include "SplinePath.h"

class CubicBSpline: public SplinePath
{
	UniformCubicBSpline<Vector2>* mSpline;

	float mLength, mMaxT;

public:

	explicit CubicBSpline(std::vector<Vector2> const& points);

	~CubicBSpline();

	void setControlPoint(int index, Vector2 const& position);

	Vector2 getPosition(float distance) const;

	Vector2 getDirection(float distance) const;

	Vector2 getAcceleration(float distance) const;

	float getLength() const;
};

