#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/TextureRenderer.h>

#include <mpp/helper/QuadBatchDataProvider.h>
#include <mpp/helper/TriangleBatchDataProvider.h>
#include <mpp/helper/TriangleBatchRenderer.h>

#include <mpp/mesh/VertexTypeSpecification.h>

#include "Control.h"

class CircleDataProvider : public mpp::helper::TriangleBatchDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>
{
	float mRadius;

	std::vector<Control*> mControls;

	std::vector<Vector2> mVertices;

	bool mDirty{ true };

public:

	CircleDataProvider(float radius, std::vector<Control*> controls)
		: mRadius(radius)
		, mControls(controls)
	{
		setNumTriangles(36);
		update(0.0f);
	}

	void position(uint32_t index, float& x0, float& y0, float& x1, float& y1, float& x2, float& y2)
	{
		x0 = mVertices[index * 3 + 0].x;
		y0 = mVertices[index * 3 + 0].y;
		x1 = mVertices[index * 3 + 1].x;
		y1 = mVertices[index * 3 + 1].y;
		x2 = mVertices[index * 3 + 2].x;
		y2 = mVertices[index * 3 + 2].y;
	}

	mpp::Colour diffuse()
	{
		return mpp::Colour::White;
	}

	void update(float frameTime)
	{
		if (mDirty)
		{
			mVertices.clear();

			float offset = mRadius;
			Vector2 vertex(0, mRadius);

			size_t numTris = getNumTriangles();
			float angleInc = 360.0f / numTris;
			for (size_t i = 0; i < numTris; ++i)
			{
				mVertices.push_back(Vector2(offset, offset));

				mVertices.push_back(vertex + offset);

				vertex.rotateClockwise(angleInc);
				mVertices.push_back(vertex + offset);
			}

			mDirty = false;
		}
	}
};

class CircleRenderer : public mpp::TextureRenderer
{
	std::shared_ptr<mpp::helper::TriangleBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>> mTriRenderer{ nullptr };

	std::shared_ptr<CircleDataProvider> mDataProvider;

private:

	void render(size_t width, size_t height) override
	{
		mTriRenderer->render();
	}

public:

	CircleRenderer(std::string const& name, std::shared_ptr<CircleDataProvider> dataProvider, mpp::RenderSystem* renderSystem, mpp::ResourceManager* resourceMgr)
		: mpp::TextureRenderer(name, renderSystem, resourceMgr)
		, mDataProvider(dataProvider)
	{
		mpp::helper::TriangleBatchRendererParams params
		{
			true,
			false,
			false
		};

		mTriRenderer = std::make_shared<mpp::helper::TriangleBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>>(
			"CircleRenderer",
			params,
			mDataProvider,
			nullptr,
			mRenderSystem,
			mResourceMgr);

		mTriRenderer->create();
	}

	void update(float frameTime)
	{
		mDataProvider->update(frameTime);
		mTriRenderer->update(mDataProvider->getNumTriangles());
	}
};

// Controls
class ControlHandlesDataProvider : public mpp::helper::QuadBatchDataProvider<mpp::mesh::DataTypeFloat,	mpp::mesh::DataTypeFloat>
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	std::vector<Control*> mControls;

	bool mDirty{ true };

	std::vector<Vector2> mVertices;

public:

	ControlHandlesDataProvider(mpp::RenderSystem* renderSystem, std::vector<Control*> controls)
		: mRenderSystem(renderSystem)
		, mControls(controls)
	{
		setNumQuads(0);
	}

	void position(uint32_t index, float& x, float& y)
	{
		if (index < getNumQuads())
		{
			x = mVertices[index].x;
			y = mVertices[index].y;
		}
	}

	void angle(uint32_t index, float& angle)
	{
		angle = 0.0f;
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

	void update(float frameTime)
	{
		if (mDirty)
		{
			mVertices.clear();

			// Regen if controls changed
			for (auto control: mControls)
			{
				auto value = control->getValue();
				auto const& pos = control->getPosition();

				switch (control->getOrientation())
				{
				case Control::Orientation::Horizontal:
					mVertices.push_back(Vector2(pos.x + value.x, pos.y));
					break;

				case Control::Orientation::Vertical:
					mVertices.push_back(Vector2(pos.x, pos.y + value.x));
					break;

				case Control::Orientation::Free:
					mVertices.push_back(pos + value);
					break;
				}
			}

			setNumQuads(mVertices.size());
			mDirty = false;
		}
	}
};


