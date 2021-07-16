#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/TextureRenderer.h>

#include <mpp/helper/TriangleBatchDataProvider.h>

class Test2dTriangleBatchDataProvider : public mpp::helper::TriangleBatch2DDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>
{
	struct Triangle
	{
		float px[3], py[3];
		float u[3], v[3];
	};

	bool mDirty{ true };

	std::vector<Triangle> mTriangles;

	glm::vec2 mBounds[2];

public:

	Test2dTriangleBatchDataProvider()
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

		if (mDirty)
		{
			mTriangles.clear();

			// Create triangles
			mTriangles.push_back(
				{
					{ 0.0f,  0.0f, 50.0f }, // X
					{ 0.0f, 50.0f, 0.0f }, // Y
					{ 0, 0, 1 }, // U
					{ 0, 1, 0 }  // V
				});

			setNumPrimitives(mTriangles.size());
			//mDirty = false;
		}

		return updated;
	}
};


