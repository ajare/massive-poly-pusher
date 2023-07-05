#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/TextureRenderer.h>

#include <mpp/helper/QuadBatchDataProvider.h>
#include <mpp/helper/TriangleBatchDataProvider.h>
#include <mpp/helper/TriangleBatchRenderer.h>

#include <mpp/mesh/VertexTypeSpecification.h>

#define DEGTORAD(d) ((d) * 3.14159f / 180.0f)
#define RADTODEG(d) ((d) * 180.0f / 3.14159f)

class BulletsByDirQuadBatchDataProvider : public mpp::helper::QuadBatchDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>
{
	struct Bullet
	{
		Vector2 pos, dir;
		float speed;
	};

private:
	
	std::vector<Bullet> mBullets;

	int mX, mY, mWidth, mHeight;

	float mTotalTime;

public:

	BulletsByDirQuadBatchDataProvider(int x, int y, int width, int height, size_t count)
		: mX(x)
		, mY(y)
		, mWidth(width)
		, mHeight(height)
		, mTotalTime(0.0f)
	{
		mBullets.resize(count);
		setNumPrimitives(0);
	}

	void getBounds(glm::vec3& bMin, glm::vec3& bMax) override
	{
		bMin.x = bMin.y = bMin.z = -1e10f;
		bMax.x = bMax.y = bMax.z = 1e10f;
	}

	void position(uint32_t index, float& x, float& y)
	{
		x = mBullets[index].pos.x;
		y = mBullets[index].pos.y;
	}

	void angle(uint32_t index, float& angle)
	{
		angle = 90 - RADTODEG(atan2(mBullets[index].dir.y, mBullets[index].dir.x));
	}

	void direction(uint32_t index, float& x, float& y)
	{
		x = mBullets[index].dir.x;
		y = mBullets[index].dir.y;
	}

	void textureAtlasTexcoords(uint32_t index, float& u0, float& v0, float& u1, float& v1)
	{
		u0 = (index % 3) / 8.0f;
		v0 = 0.0f;
		u1 = ((index % 3) + 1) / 8.0f;
		v1 = 1.0f;
	}

	void radius(uint32_t index, float& radiusX, float& radiusY)
	{
		radiusX = 16;
		radiusY = 16;
	}

	mpp::Colour diffuse()
	{
		return mpp::Colour::White;
	}

	bool update(float frameTime)
	{
		mTotalTime += frameTime;

		float cx = (float)(mX + mWidth / 2);
		float cy = (float)(mY + mHeight / 2);
		float radius = std::min(mWidth, mHeight) * 0.5f * 0.9f;

		auto count = mBullets.size();
		for (size_t i = 0; i < count; ++i)
		{
			auto& bullet = mBullets[i];

			float angle = mTotalTime * 20 + (i * 360.0f) / count;

			float sinAngle = sin(DEGTORAD(angle));
			float cosAngle = cos(DEGTORAD(angle));

			bullet.pos.x = cx + sinAngle * radius;
			bullet.pos.y = cy + cosAngle * radius;
			bullet.dir.x = sinAngle;
			bullet.dir.y = cosAngle;
		}

		setNumPrimitives(count);

		return true;
	}
};

class BulletsByAngleQuadBatchDataProvider : public mpp::helper::QuadBatchDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>
{
	struct Bullet
	{
		Vector2 pos;
		float angle;
	};

private:

	std::vector<Bullet> mBullets;

	int mX, mY, mWidth, mHeight;

	float mTotalTime;

public:

	BulletsByAngleQuadBatchDataProvider(int x, int y, int width, int height, size_t count)
		: mX(x)
		, mY(y)
		, mWidth(width)
		, mHeight(height)
		, mTotalTime(0.0f)
	{
		mBullets.resize(count);
		setNumPrimitives(0);
	}

	void getBounds(glm::vec3& bMin, glm::vec3& bMax) override
	{
		bMin.x = bMin.y = bMin.z = -1e10f;
		bMax.x = bMax.y = bMax.z = 1e10f;
	}

	void position(uint32_t index, float& x, float& y)
	{
		x = mBullets[index].pos.x;
		y = mBullets[index].pos.y;
	}

	void angle(uint32_t index, float& angle)
	{
		angle = mBullets[index].angle;
	}

	void direction(uint32_t index, float& x, float& y)
	{
		float a;

		angle(index, a);
		x = sinf(a * 3.14159f / 180);
		y = cosf(a * 3.14159f / 180);
	}

	void textureAtlasTexcoords(uint32_t index, float& u0, float& v0, float& u1, float& v1)
	{
		u0 = (index % 3) / 8.0f;
		v0 = 0.0f;
		u1 = ((index % 3) + 1) / 8.0f;
		v1 = 1.0f;
	}

	void radius(uint32_t index, float& radiusX, float& radiusY)
	{
		radiusX = 16;
		radiusY = 16;
	}

	mpp::Colour diffuse()
	{
		return mpp::Colour::White;
	}

	bool update(float frameTime)
	{
		mTotalTime += frameTime;

		float cx = (float)(mX + mWidth / 2);
		float cy = (float)(mY + mHeight / 2);
		float radius = std::min(mWidth, mHeight) * 0.5f * 0.9f;

		auto count = mBullets.size();
		for (size_t i = 0; i < count; ++i)
		{
			auto& bullet = mBullets[i];

			float angle = mTotalTime * 20 + (i * 360.0f) / count;

			bullet.pos.x = cx + sin(DEGTORAD(angle)) * radius;
			bullet.pos.y = cy + cos(DEGTORAD(angle)) * radius;
			bullet.angle = angle + 90;
		}

		setNumPrimitives(count);

		return true;
	}
};

class TestDragonDataProvider : public mpp::helper::QuadBatchDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	bool mDirty{ true };

	std::vector<float> mVertexData;

	float mTotalTime;

	size_t mCount;

	bool mRotate;

public:

	TestDragonDataProvider(mpp::RenderSystem* renderSystem, size_t count, bool rotate)
		: mRenderSystem(renderSystem)
		, mCount(count)
		, mRotate(rotate)
		, mTotalTime(0.0f)
	{
		setNumPrimitives(0);
	}

	void getBounds(glm::vec3& bMin, glm::vec3& bMax) override
	{
		bMin.x = bMin.y = bMin.z = -1e10f;
		bMax.x = bMax.y = bMax.z = 1e10f;
	}

	void position(uint32_t index, float& x, float& y)
	{
		if (index < getNumPrimitives())
		{
			x = mVertexData[index * 2 + 0];
			y = mVertexData[index * 2 + 1];
		}
	}

	void angle(uint32_t index, float& angle)
	{
		if (mRotate)
		{
			angle = mTotalTime;
		}
		else
		{
			angle = 0.0f;
		}
	}

	void direction(uint32_t index, float& x, float& y)
	{
		float a;

		angle(index, a);
		x = sinf(a * 3.14159f / 180);
		y = cosf(a * 3.14159f / 180);
	}

	void textureAtlasTexcoords(uint32_t index, float& u0, float& v0, float& u1, float& v1)
	{
		u0 = 0.0f;
		v0 = 0.0f;
		u1 = 1.0f;
		v1 = 1.0f;
	}

	void radius(uint32_t index, float& radiusX, float& radiusY)
	{
		radiusX = 128.0f;
		radiusY = 128.0f;
	}

	mpp::Colour diffuse()
	{
		return mpp::Colour::White;
	}

	void setDirty()
	{
		mDirty = true;
	}

	bool update(float frameTime)
	{
		bool updated = mDirty;
		mTotalTime += frameTime;

		if (mDirty)
		{
			mVertexData.clear();

			mVertexData.push_back(400);
			mVertexData.push_back(300);

			setNumPrimitives(mCount);
			//mDirty = false;
		}

		return updated;
	}
};

class TestAtlasDataProvider : public mpp::helper::QuadBatchDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	bool mDirty{ true };

	std::vector<float> mVertexData;

	float mTotalTime;

	size_t mCount;

	bool mRotate;

public:

	TestAtlasDataProvider(mpp::RenderSystem* renderSystem, size_t count, bool rotate)
		: mRenderSystem(renderSystem)
		, mCount(count)
		, mRotate(rotate)
		, mTotalTime(0.0f)
	{
		setNumPrimitives(0);
	}

	void getBounds(glm::vec3& bMin, glm::vec3& bMax) override
	{
		bMin.x = bMin.y = bMin.z = -1e10f;
		bMax.x = bMax.y = bMax.z = 1e10f;
	}

	void position(uint32_t index, float& x, float& y)
	{
		if (index < getNumPrimitives())
		{
			x = mVertexData[index * 2 + 0];
			y = mVertexData[index * 2 + 1];
		}
	}

	void angle(uint32_t index, float& angle)
	{
		if (mRotate)
		{
			angle = mTotalTime;
		}
		else
		{
			angle = 0.0f;
		}
	}

	void direction(uint32_t index, float& x, float& y)
	{
		float a;

		angle(index, a);
		x = sinf(a * 3.14159f / 180);
		y = cosf(a * 3.14159f / 180);
	}

	void textureAtlasTexcoords(uint32_t index, float& u0, float& v0, float& u1, float& v1)
	{
		u0 = 0.0f;
		v0 = 0.0f;
		u1 = 0.5f;
		v1 = 1.0f;
	}

	void radius(uint32_t index, float& radiusX, float& radiusY)
	{
		radiusX = 128.0f;
		radiusY = 128.0f;
	}

	mpp::Colour diffuse()
	{
		return mpp::Colour::White;
	}

	void setDirty()
	{
		mDirty = true;
	}

	bool update(float frameTime)
	{
		bool updated = mDirty;
		mTotalTime += frameTime;

		if (mDirty)
		{
			mVertexData.clear();

			mVertexData.push_back(256);
			mVertexData.push_back(128);

			setNumPrimitives(mCount);
			//mDirty = false;
		}

		return updated;
	}
};
