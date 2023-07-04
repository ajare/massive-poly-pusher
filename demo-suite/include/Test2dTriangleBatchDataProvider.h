#pragma once

#include <cmath>

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/TextureRenderer.h>

#include <mpp/helper/TriangleBatchDataProvider.h>

#include "Vector2.h"

#define DEGTORAD(d) ((d) * 3.14159f / 180.0f)

class Test2dTriangleBatchDataProvider : public mpp::helper::TriangleBatch2DDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>
{
	struct Point
	{
		Vector2 pos, dir;
	};

	struct Triangle
	{
		Vector2 p[3];
	};

private:

	const int NumPoints = 90;

	std::vector<Point> mPoints;

	std::vector<Triangle> mTriangles;

	glm::vec2 mBounds[2];

	int mX, mY, mWidth, mHeight;

	float mTotalTime;

public:

	Test2dTriangleBatchDataProvider(int x, int y, int width, int height)
		: mX(x)
		, mY(y)
		, mWidth(width)
		, mHeight(height)
		, mTotalTime(0.0f)
	{
		// Generate points in a grid, one point per cell
		// Assign each point a cardinal direction
		// Each update, move the point in that direction, and
		// do a random check.  If that check passes, rotate direction
		// by 90 degrees.
		// If a point goes out of bounds, move back and rotate direction.
		// To colour, use blank texture, and set triangle colour
		// based on its size.

		int w2 = width * 0.8f;
		int h2 = height * 0.8f;
		for (int i = 0; i < NumPoints; ++i)
		{
			int xp = rand() % w2;
			int yp = rand() % h2;

			Vector2 p(x + xp + width * 0.1f, y + yp + height * 0.1f);

			int angle = rand() % 360;
			Vector2 d(sinf(DEGTORAD(angle)), cosf(DEGTORAD(angle)));

			Point point{
				p, d
			};

			mPoints.push_back(point);
		}

		update(0.0f);
	}

	void getBounds(glm::vec3& bMin, glm::vec3& bMax) override
	{
		bMin.x = bMin.y = bMin.z = -1e10f;
		bMax.x = bMax.y = bMax.z = 1e10f;
	}

	void position(uint32_t index, float& x0, float& y0, float& x1, float& y1, float& x2, float& y2) override
	{
		if (index < getNumPrimitives())
		{
			x0 = mTriangles[index].p[0].x;
			y0 = mTriangles[index].p[0].y;
			x1 = mTriangles[index].p[1].x;
			y1 = mTriangles[index].p[1].y;
			x2 = mTriangles[index].p[2].x;
			y2 = mTriangles[index].p[2].y;
		}
	}

	void texcoords(uint32_t index, float& u0, float& v0, float& u1, float& v1, float& u2, float& v2) override
	{
		if (index < getNumPrimitives())
		{
			u0 = 0;
			v0 = 0;
			u1 = 1;
			v1 = 0;
			u2 = 0.5f;
			v2 = 1;
		}
	}

	void colour(uint32_t index, uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha) override
	{
		auto const& tri = mTriangles[index];

		// Area
		Vector2 a = tri.p[1] - tri.p[0];
		Vector2 b = tri.p[2] - tri.p[0];
		float area = fabs(a.x * b.y - a.y * b.x) / 2.0f;

		float cellArea2 = mWidth * mHeight * 0.5f;
		uint8_t d = (uint8_t)(255.0f * area / cellArea2);

		uint8_t dr = (uint8_t)((sinf(mTotalTime) + 1.0f) * 127);

		switch (index % 3)
		{
		case 0:
			red = dr;
			green = (d * 1023) & 255;
			blue = 255 - (d % 47);
			break;
		case 1:
			red = 255 - (d % 47);
			green = 255 - d;
			blue = (d * 1719) & 255;
			break;
		case 2:
			red = d;
			green = dr;
			blue = rand() % 255;
			break;
		}

		alpha = 128;
	}

	mpp::Colour diffuse() override
	{
		return mpp::Colour::White;
	}

	bool update(float frameTime)
	{
		mTotalTime += frameTime;

		// Update points
		for (auto& point : mPoints)
		{
			bool newDir{ true };
			
			Vector2 p = point.pos + point.dir * 20 * frameTime;
			if (p.x >= mX && p.x < (mX + mWidth) && p.y >= mY && p.y < (mY + mHeight))
			{
				point.pos = p;
				newDir = rand() % 1000 > 999;
			}

			if (newDir)
			{
				point.dir = point.dir.perpendicular();
			}
		}

		// Triangulate
		mTriangles.clear();

		for (int i = 0; i < NumPoints; i += 3)
		{
			Triangle t
			{
				{
					mPoints[i + 0].pos,
					mPoints[i + 1].pos,
					mPoints[i + 2].pos
				}
			};

			mTriangles.push_back(t);
		}

		setNumPrimitives(mTriangles.size());

		return true;
	}
};


