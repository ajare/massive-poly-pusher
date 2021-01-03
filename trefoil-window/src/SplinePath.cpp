#include "SplinePath.h"

using namespace std;

SplinePath::SplinePath(vector<Vector2> const& points)
	: mPoints(points)
{
	int numPoints = (int)points.size();
	if (numPoints < 4)
	{
		throw exception("SplinePath: cubic curves require at least 4 points.");
	}
}

int SplinePath::getNumControlPoints() const
{
	return (int)mPoints.size();
}

Vector2 const& SplinePath::getControlPoint(int index) const
{
	return mPoints[index];
}

void SplinePath::setControlPoint(int index, Vector2 const& position)
{
	mPoints[index] = position;
}

vector<Vector2> SplinePath::divide(bool adaptive, float scale) const
{
	vector<Vector2> vertices;

	float length = getLength();

	int n = (int)(length * scale);
	float t = 0.0f, dt = 1.0f / (n - 1);

	for (int i = 0; i < n; ++i)
	{
		vertices.push_back(getPosition(t * length));
		t += dt;
	}

	return vertices;
}

