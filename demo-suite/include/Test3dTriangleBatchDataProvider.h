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

	bool update(float frameTime) override
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

class Test3dTriangleBatchBufferDataProvider : public mpp::helper::TriangleBatch3DBufferDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>
{
	struct DrawVert
	{
		float pos[3];
		float nor[3];
		float tex[2];
		uint8_t col[4];
	};

private:

	float mTotalTime;

	uint32_t mVertexStride;

	uint32_t mNumVertices;

	uint32_t mNumTriangles;

	uint32_t mVertexDataSize;

	int8_t* mVertexData;

	uint16_t* mIndexData;

	DrawVert* _workVert;

	uint16_t* _workIndex;

private:

	void addTriangle(uint16_t v0, uint16_t v1, uint16_t v2)
	{
		*_workIndex++ = v0;
		*_workIndex++ = v1;
		*_workIndex++ = v2;
	}

	void getUvCoord(double nx, double ny, double nz, double* u, double* v)
	{
		double phi = ny;
		double lambda;
		if (ny == 1.0 || ny == -1.0)
		{
			// At a pole.
			lambda = 0.0;
		}
		else
		{
			lambda = atan2(nz, nx);
		}

		*u = -((lambda / 3.14159) * 0.5 + 0.5);
		*u *= 2; // stretch for 2-1 ratio;

		*v = tan(phi);
	}

public:

	// This is wrong for a cube - indexed vertices are a bad choice here
	// because each side has its own normals and texture coords.  Create
	// a sphere instead - copy code from SphereModelStream
	Test3dTriangleBatchBufferDataProvider()
		: mTotalTime(0.0f)
		, mVertexStride(sizeof(float) * 8 + sizeof(uint8_t) * 4)
		, mNumVertices(12)
		, mNumTriangles(20)
		, _workVert{ nullptr }
		, _workIndex{ nullptr }
	{
		mVertexDataSize = mNumVertices * mVertexStride;
		mVertexData = new int8_t[mVertexDataSize];
		mIndexData = new uint16_t[mNumTriangles * 3];

		update(0.0f);
	}

	~Test3dTriangleBatchBufferDataProvider()
	{
		delete[] mVertexData;
		delete[] mIndexData;
	}

	void getBounds(glm::vec3& bMin, glm::vec3& bMax) override
	{
		bMin.x = bMin.y = bMin.z = -1e10f;
		bMax.x = bMax.y = bMax.z = 1e10f;
	}

	uint32_t getNumVertices() const override
	{
		return mNumVertices;
	}

	int8_t* getVertexData(uint32_t meshIndex) const override
	{
		return mVertexData;
	}

	uint32_t getVertexDataSize(uint32_t meshIndex) const override
	{
		return mVertexDataSize;
	}

	int8_t* getIndexData(uint32_t meshIndex) const override
	{
		return (int8_t*)mIndexData;
	}

	uint32_t getNumIndices(uint32_t meshIndex) const override
	{
		return mNumTriangles * 3;
	}

	uint32_t getIndexWidth() const override
	{
		return 16;
	}

	mpp::Colour diffuse() override
	{
		return mpp::Colour::White;
	}

	bool update(float frameTime) override
	{
		setNumPrimitives(mNumTriangles);
		_workVert = (DrawVert*)mVertexData;
		_workIndex = (uint16_t*)mIndexData;

		addTriangle(1, 4, 0);
		addTriangle(4, 9, 0);
		addTriangle(4, 5, 9);
		addTriangle(8, 5, 4);
		addTriangle(1, 8, 4);
		addTriangle(1, 10, 8);
		addTriangle(10, 3, 8);
		addTriangle(8, 3, 5);
		addTriangle(3, 2, 5);
		addTriangle(3, 7, 2);
		addTriangle(3, 10, 7);
		addTriangle(10, 6, 7);
		addTriangle(6, 11, 7);
		addTriangle(6, 0, 11);
		addTriangle(6, 1, 0);
		addTriangle(10, 1, 6);
		addTriangle(11, 0, 9);
		addTriangle(2, 11, 9);
		addTriangle(5, 2, 9);
		addTriangle(11, 2, 7);

		// Vertex data format is Pos(float3), Normal(float*3), Texture(float*2), Colour(uint8*4)
		// Index format is int16
		float scale = 20.0f + sin(mTotalTime * 2.3f) * 80.0f;

		// Generate vertices
		double x = 0.525731112119133606;
		double z = 0.850650808352039932;

		std::vector<double> positions =
		{
			-x, 0, z,
			x, 0, z,
			-x, 0, -z,
			x, 0, -z,
			0, z, x,
			0, z, -x,
			0, -z, x,
			0, -z, -x,
			z, x, 0,
			-z, x, 0,
			z, -x, 0,
			-z, -x, 0
		};

		for (size_t i = 0; i < positions.size(); i += 3)
		{
			// Position3
			_workVert->pos[0] = (float)positions[i + 0] * scale;
			_workVert->pos[1] = (float)positions[i + 1] * scale;
			_workVert->pos[2] = (float)positions[i + 2] * scale;

			// Normal3
			auto len = sqrtf((float)(positions[i + 0] * positions[i + 0] +
				positions[i + 1] * positions[i + 1] +
				positions[i + 2] * positions[i + 2]));

			_workVert->nor[0] = (float)positions[i + 0] / len;
			_workVert->nor[1] = (float)positions[i + 1] / len;
			_workVert->nor[2] = (float)positions[i + 2] / len;

			// Texcoord2
			double u, v;
			getUvCoord(_workVert->nor[0], _workVert->nor[1], _workVert->nor[2], &u, &v);

			_workVert->tex[0] = (float)u;
			_workVert->tex[1] = (float)v;

			// Colour4
			_workVert->col[0] = 255;
			_workVert->col[1] = 255;
			_workVert->col[2] = 255;
			_workVert->col[3] = 255;

			_workVert++;
		}

		mTotalTime += frameTime;
		return true;
	}
};

