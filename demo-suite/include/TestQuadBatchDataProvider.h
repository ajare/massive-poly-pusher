#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/TextureRenderer.h>

#include <mpp/helper/QuadBatchDataProvider.h>
#include <mpp/helper/TriangleBatchDataProvider.h>
#include <mpp/helper/TriangleBatchRenderer.h>

#include <mpp/mesh/VertexTypeSpecification.h>

class TestQuadBatchDataProvider : public mpp::helper::QuadBatchDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	bool mDirty{ true };

	std::vector<float> mVertexData;

	float mTotalTime;

	float mRadius;

	size_t mCount;

	int mTextureIndex;

	bool mRotate;

public:

	TestQuadBatchDataProvider(mpp::RenderSystem* renderSystem, float radius, size_t count, int textureIndex, bool rotate)
		: mRenderSystem(renderSystem)
		, mCount(count)
		, mRadius(radius)
		, mTextureIndex(textureIndex)
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

	void textureAtlasTexcoords(uint32_t index, float& u0, float& v0, float& u1, float& v1)
	{
		if (mTextureIndex >= 0)
		{
			u0 = (index % 3) / 8.0f;
			v0 = 0.0f;
			u1 = ((index % 3) + 1) / 8.0f;
			v1 = 1.0f;
		}
		else
		{
			u0 = 0.0f;
			v0 = 0.0f;
			u1 = 1.0f;
			v1 = 1.0f;
		}
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

			float cx = 400;
			float cy = 300;

			for (size_t i = 0; i < mCount; ++i)
			{
				float angle = (i * 2 * 3.14159f) / mCount;
				mVertexData.push_back(cx + sin(angle + mTotalTime) * mRadius);
				mVertexData.push_back(cy + cos(angle + mTotalTime) * mRadius);
			}

			setNumPrimitives(mCount);
			//mDirty = false;
		}

		return updated;
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

	void textureAtlasTexcoords(uint32_t index, float& u0, float& v0, float& u1, float& v1)
	{
		u0 = 0.0f;
		v0 = 0.0f;
		u1 = 1.0f;
		v1 = 1.0f;
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

