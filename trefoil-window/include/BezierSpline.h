#pragma once

#include <vector>

#include "SplinePath.h"

class BezierSpline : public SplinePath
{
	struct Segment
	{
		float offset, length;
	};

private:

	int mRecursionLimit;

	float mScale, mPathEpsilon, mAngleToleranceEpsilon, mAngleTolerance, mCuspLimit;

	mutable std::vector<Segment> mSegments;

private:

	void createSegments() const;

	void divideAdaptive(std::vector<Vector2>& vertices, Vector2 const& v1, Vector2 const& v2, Vector2 const& v3, Vector2 const& v4, float tolerance, int depth) const;

	void divideEqual(std::vector<Vector2>& vertices, int segment) const;

	int getSegmentIndex(float distance) const;

	Segment const& getSegment(int index) const;

	float calculateSegmentLength(int point) const;

protected:

	Vector2 getPosition(int point, float t) const;

	Vector2 get1stDerivative(int point, float t) const;

	Vector2 get2ndDerivative(int point, float t) const;

	float getCurvature(int point, float t) const;

public:

	explicit BezierSpline(std::vector<Vector2> const& points);

	void setControlPoint(int index, Vector2 const& position);

	std::vector<Vector2> divide(bool adaptive, float scale = 1.0f) const;
			
	Vector2 getPosition(float distance) const;

	Vector2 getDirection(float distance) const;

	Vector2 getAcceleration(float distance) const;

	float getLength() const;
};

