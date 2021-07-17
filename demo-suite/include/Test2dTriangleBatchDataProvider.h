#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/TextureRenderer.h>

#include <mpp/helper/TriangleBatchDataProvider.h>

#include <spline_library/splines/cubic_hermite_spline.h>

#include "Vector2.h"

class Test2dTriangleBatchDataProvider : public mpp::helper::TriangleBatch2DDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>
{
	struct Triangle
	{
		float px[3], py[3];
		float u[3], v[3];
	};

	bool mDirty{ true };

	float mTotalTime{ 0.0f };

	LoopingCubicHermiteSpline<Vector2> mSpline;

	std::vector<Triangle> mTriangles;

	glm::vec2 mBounds[2];

public:

	Test2dTriangleBatchDataProvider()
		: mSpline({ { 150, 150 }, { 150, 450 }, { 650, 450 }, {550, 50 } }, 0.5f)
	{
		update(0.0f);
	}

	void setDirty()
	{
		mDirty = true;
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
			x0 = mTriangles[index].px[0];
			y0 = mTriangles[index].py[0];
			x1 = mTriangles[index].px[1];
			y1 = mTriangles[index].py[1];
			x2 = mTriangles[index].px[2];
			y2 = mTriangles[index].py[2];
		}
	}

	void texcoords(uint32_t index, float& u0, float& v0, float& u1, float& v1, float& u2, float& v2) override
	{
		if (index < getNumPrimitives())
		{
			u0 = mTriangles[index].u[0];
			v0 = mTriangles[index].v[0];
			u1 = mTriangles[index].u[1];
			v1 = mTriangles[index].v[1];
			u2 = mTriangles[index].u[2];
			v2 = mTriangles[index].v[2];
		}
	}

	void colour(uint32_t index, uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha) override
	{
		red = 255;
		green = 255;
		blue = 255;
		alpha = 255;
	}

	mpp::Colour diffuse() override
	{
		return mpp::Colour::White;
	}

	bool update(float frameTime)
	{
		bool updated = mDirty;
		mTotalTime += frameTime;

		if (mDirty)
		{
			mTriangles.clear();

			auto maxLength = mSpline.totalLength();
			auto maxT = mSpline.getMaxT();

			const size_t numSegs{ 50 };
			float slen{ maxLength / 2 };

			// Create points
			std::vector<Vector2> points;
			float sp = mTotalTime;
			while (sp > maxLength)
			{
				sp -= maxLength;
			}

			float st = slen / maxLength;
			for (size_t i = 0; i <= numSegs; ++i)
			{
				float t = sp + st * (i / (float)numSegs);
				if (t > 1.0f)
				{
					t -= 1.0f;
				}

				auto pos = mSpline.getPosition(t * maxT);
				auto dir = mSpline.getTangent(t * maxT).tangent;

				points.push_back(pos);
				points.push_back(dir.perpendicular().normalisedCopy());
			}

			// Create triangles

			const float width{ 20.0f };
			for (size_t i = 0; i < numSegs; ++i)
			{
				float w2 = width / 2;
				auto const& p0 = points[(i + 0) * 2 + 0];
				auto const& p1 = points[(i + 1) * 2 + 0];
				auto const& t0 = points[(i + 0) * 2 + 1];
				auto const& t1 = points[(i + 1) * 2 + 1];

				mTriangles.push_back(
					{
						{ p0.x - t0.x * w2, p1.x - t1.x * w2, p1.x + t1.x * w2 }, // X
						{ p0.y - t0.y * w2, p1.y - t1.y * w2, p1.y + t1.y * w2 }, // Y
						{ 0, 0, 1 }, // U
						{ 0, 0, 1 }  // V
					});
				mTriangles.push_back(
					{
						{ p1.x + t1.x * w2, p0.x + t0.x * w2, p0.x - t0.x * w2 }, // X
						{ p1.y + t1.y * w2, p0.y + t0.y * w2, p0.y - t0.y * w2 }, // Y
						{ 1, 1, 0 }, // U
						{ 1, 0, 0 }  // V
					});
			}

			setNumPrimitives(mTriangles.size());
			//mDirty = false;
		}

		return updated;
	}
};


