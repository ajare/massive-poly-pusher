#pragma once

#include <vector>

#include "Vector2.h"

class SplinePath
{
protected:

	std::vector<Vector2> mPoints;

public:

	explicit SplinePath(std::vector<Vector2> const& points);

	virtual std::vector<Vector2> divide(bool adaptive, float scale = 1.0f) const;

	int getNumControlPoints() const;

	Vector2 const& getControlPoint(int index) const;

	virtual void setControlPoint(int index, Vector2 const& position);

	virtual Vector2 getPosition(float distance) const = 0;

	virtual Vector2 getDirection(float distance) const = 0;

	virtual Vector2 getAcceleration(float distance) const = 0;

	virtual float getLength() const = 0;
};

