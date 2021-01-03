#pragma once
#pragma once

#include <vector>

#pragma warning(push)
#pragma warning(disable: 4244)
#include <spline_library/splines/cubic_hermite_spline.h>
#pragma warning(pop)

#include "SplinePath.h"

class CentripetalCatmullRomSpline : public SplinePath
{
	CubicHermiteSpline<Vector2>* mSpline;

	float mLength, mMaxT;

public:

	explicit CentripetalCatmullRomSpline(std::vector<Vector2> const& points);

	~CentripetalCatmullRomSpline();

	void setControlPoint(int index, Vector2 const& position);

	Vector2 getPosition(float distance) const;

	Vector2 getDirection(float distance) const;

	Vector2 getAcceleration(float distance) const;

	float getLength() const;
};

