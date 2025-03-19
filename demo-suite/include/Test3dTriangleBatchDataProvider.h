#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/TextureRenderer.h>

#include <mpp/helper/TriangleBatchDataProvider.h>

class Test3dTriangleBatchDataProvider : public mpp::helper::TriangleBatch3DDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>
{
	struct Triangle
	{
		float px[3], py[3], pz[3];
		float nx[3], ny[3], nz[3];
		float u[3], v[3];
	};
	
	float mTotalTime;

	std::vector<Triangle> mTriangles;

public:

	Test3dTriangleBatchDataProvider()
		: mTotalTime(0.0f)
	{
		update(0.0f);
	}

	void getBounds(glm::vec3& bMin, glm::vec3& bMax) override
	{
		bMin.x = bMin.y = bMin.z = -1e10f;
		bMax.x = bMax.y = bMax.z = 1e10f;
	}

	void position(uint32_t index, float& x0, float& y0, float& z0, float& x1, float& y1, float& z1, float& x2, float& y2, float& z2) override
	{
		if (index < getNumPrimitives())
		{
			x0 = mTriangles[index].px[0];
			y0 = mTriangles[index].py[0];
			z0 = mTriangles[index].pz[0];
			x1 = mTriangles[index].px[1];
			y1 = mTriangles[index].py[1];
			z1 = mTriangles[index].pz[1];
			x2 = mTriangles[index].px[2];
			y2 = mTriangles[index].py[2];
			z2 = mTriangles[index].pz[2];
		}
	}

	void normal(uint32_t index, float& x0, float& y0, float& z0, float& x1, float& y1, float& z1, float& x2, float& y2, float& z2) override
	{
		if (index < getNumPrimitives())
		{
			x0 = mTriangles[index].nx[0];
			y0 = mTriangles[index].ny[0];
			z0 = mTriangles[index].nz[0];
			x1 = mTriangles[index].nx[1];
			y1 = mTriangles[index].ny[1];
			z1 = mTriangles[index].nz[1];
			x2 = mTriangles[index].nx[2];
			y2 = mTriangles[index].ny[2];
			z2 = mTriangles[index].nz[2];
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
		float scale = 20.0f + sin(mTotalTime * 2.3f) * 80.0f;

		mTriangles.clear();

		// Create triangles
		mTriangles.push_back(
			{
				{ -1 * scale,  1 * scale,  1 * scale }, // X
				{ -1 * scale, -1 * scale,  1 * scale }, // Y
				{ -1 * scale, -1 * scale, -1 * scale }, // Z
				{ 0, 0, 0 }, // NX
				{ 0, 0, 0 }, // NY
				{ -1, -1, -1 }, // NZ
				{ 0, 1, 1 }, // U
				{ 0, 0, 1 }  // V
			});

		mTriangles.push_back(
			{
				{  1 * scale, -1 * scale, -1 * scale }, // X
				{  1 * scale,  1 * scale, -1 * scale }, // Y
				{ -1 * scale, -1 * scale, -1 * scale }, // Z
				{ 0, 0, 0 }, // NX
				{ 0, 0, 0 }, // NY
				{ -1, -1, -1 }, // NZ
				{ 1, 0, 0 }, // U
				{ 1, 1, 0 }  // V
			});

		mTriangles.push_back(
			{
				{ -1 * scale,  1 * scale,  1 * scale }, // X
				{ -1 * scale, -1 * scale,  1 * scale }, // Y
				{  1 * scale,  1 * scale,  1 * scale }, // Z
				{ 0, 0, 0 }, // NX
				{ 0, 0, 0 }, // NY
				{ 1, 1, 1 }, // NZ
				{ 0, 1, 1 }, // U
				{ 0, 0, 1 }  // V
			});

		mTriangles.push_back(
			{
				{  1 * scale, -1 * scale, -1 * scale }, // X
				{  1 * scale,  1 * scale, -1 * scale }, // Y
				{  1 * scale,  1 * scale,  1 * scale }, // Z
				{ 0, 0, 0 }, // NX
				{ 0, 0, 0 }, // NY
				{ 1, 1, 1 }, // NZ
				{ 1, 0, 0 }, // U
				{ 1, 1, 0 }  // V
			});

		mTriangles.push_back(
			{
				{  1 * scale,  1 * scale,  1 * scale }, // X
				{ -1 * scale,  1 * scale,  1 * scale }, // Y
				{ -1 * scale, -1 * scale,  1 * scale }, // Z
				{ 1, 1, 1 }, // NX
				{ 0, 0, 0 }, // NY
				{ 0, 0, 0 }, // NZ
				{ 0, 1, 1 }, // U
				{ 0, 0, 1 }  // V
			});

		mTriangles.push_back(
			{
				{  1 * scale,  1 * scale,  1 * scale }, // X
				{  1 * scale, -1 * scale, -1 * scale }, // Y
				{  1 * scale,  1 * scale, -1 * scale }, // Z
				{ 1, 1, 1 }, // NX
				{ 0, 0, 0 }, // NY
				{ 0, 0, 0 }, // NZ
				{ 1, 0, 0 }, // U
				{ 1, 1, 0 }  // V
			});

		mTriangles.push_back(
			{
				{ -1 * scale, -1 * scale, -1 * scale }, // X
				{ -1 * scale,  1 * scale,  1 * scale }, // Y
				{ -1 * scale, -1 * scale,  1 * scale }, // Z
				{ -1, -1, -1 }, // NX
				{ 0, 0, 0 }, // NY
				{ 0, 0, 0 }, // NZ
				{ 0, 1, 1 }, // U
				{ 0, 0, 1 }  // V
			});

		mTriangles.push_back(
			{
				{  -1 * scale, -1 * scale, -1 * scale }, // X
				{  1 * scale, -1 * scale, -1 * scale }, // Y
				{  1 * scale,  1 * scale, -1 * scale }, // Z
				{ -1, -1, -1 }, // NX
				{ 0, 0, 0 }, // NY
				{ 0, 0, 0 }, // NZ
				{ 1, 0, 0 }, // U
				{ 1, 1, 0 }  // V
			});

		mTriangles.push_back(
			{
				{ -1 * scale,  1 * scale,  1 * scale }, // X
				{ -1 * scale, -1 * scale, -1 * scale }, // Y
				{ -1 * scale, -1 * scale,  1 * scale }, // Z
				{ 0, 0, 0 }, // NX
				{ -1, -1, -1 }, // NY
				{ 0, 0, 0 }, // NZ
				{ 0, 1, 1 }, // U
				{ 0, 0, 1 }  // V
			});

		mTriangles.push_back(
			{
				{  1 * scale, -1 * scale, -1 * scale }, // X
				{  -1 * scale, -1 * scale, -1 * scale }, // Y
				{  1 * scale,  1 * scale, -1 * scale }, // Z
				{ 0, 0, 0 }, // NX
				{ -1, -1, -1 }, // NY
				{ 0, 0, 0 }, // NZ
				{ 1, 0, 0 }, // U
				{ 1, 1, 0 }  // V
			});

		mTriangles.push_back(
			{
				{ -1 * scale,  1 * scale,  1 * scale }, // X
				{ 1 * scale, 1 * scale, 1 * scale }, // Y
				{ -1 * scale, -1 * scale,  1 * scale }, // Z
				{ 0, 0, 0 }, // NX
				{ 1, 1, 1 }, // NY
				{ 0, 0, 0 }, // NZ
				{ 0, 1, 1 }, // U
				{ 0, 0, 1 }  // V
			});

		mTriangles.push_back(
			{
				{  1 * scale, -1 * scale, -1 * scale }, // X
				{  1 * scale, 1 * scale, 1 * scale }, // Y
				{  1 * scale,  1 * scale, -1 * scale }, // Z
				{ 0, 0, 0 }, // NX
				{ 1, 1, 1 }, // NY
				{ 0, 0, 0 }, // NZ
				{ 1, 0, 0 }, // U
				{ 1, 1, 0 }  // V
			});

		setNumPrimitives(mTriangles.size());
	
		mTotalTime += frameTime;
		return true;
	}
};


