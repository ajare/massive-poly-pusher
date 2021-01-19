#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/TextureRenderer.h>

#include <mpp/helper/TriangleBatchDataProvider.h>

// Controls
class TestTriangleBatchDataProvider : public mpp::helper::TriangleBatchDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>
{
	struct Triangle
	{
		float x[3], y[3];
		float u[3], v[3];
	};
	
	bool mDirty{ true };

	std::vector<Triangle> mTriangles;

public:

	TestTriangleBatchDataProvider()
	{
		update(0.0f);
	}

	void setDirty()
	{
		mDirty = true;
	}

	void position(uint32_t index, float& x0, float& y0,	float& x1, float& y1, float& x2, float& y2) override
	{
		if (index < getNumPrimitives())
		{
			x0 = mTriangles[index].x[0];
			y0 = mTriangles[index].y[0];
			x1 = mTriangles[index].x[1];
			y1 = mTriangles[index].y[1];
			x2 = mTriangles[index].x[2];
			y2 = mTriangles[index].y[2];
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

	mpp::Colour diffuse() override
	{
		return mpp::Colour::White;
	}

	void update(float frameTime)
	{
		if (mDirty)
		{
			mTriangles.clear();

			// Create triangles
			mTriangles.push_back(
			{
				{ 400, 400, 656 }, // X
				{ 200, 456, 456 }, // Y
				{ 0, 0, 1 }, // U
				{ 0, 1, 1 }  // V
				});

			mTriangles.push_back(
				{
					{ 656, 656, 400 }, // X
					{ 456, 200, 200 }, // Y
					{ 1, 1, 0 }, // U
					{ 1, 0, 0 }  // V
				});

			setNumPrimitives(mTriangles.size());
			mDirty = false;
		}
	}
};


