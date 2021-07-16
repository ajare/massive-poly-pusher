#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>

#include <mpp/helper/LineBatchDataProvider.h>

class TestLineBatchDataProvider : public mpp::helper::LineBatchDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>
{
	struct Line
	{
		float x[2], y[2];
	};

	std::vector<Line> mLines;

	glm::vec3 mBounds[2];

	float mTotalTime;

	bool mDirty{ true };

public:

	TestLineBatchDataProvider()
		: mTotalTime(0.0f)
	{
		update(0.0f);
	}

	void getBounds(glm::vec3& bMin, glm::vec3& bMax) override
	{
		bMin = mBounds[0];
		bMax = mBounds[1];
	}

	void position(uint32_t index, float& x0, float& y0, float& x1, float& y1)
	{
		if (index < getNumPrimitives())
		{
			x0 = mLines[index].x[0];
			y0 = mLines[index].y[0];
			x1 = mLines[index].x[1];
			y1 = mLines[index].y[1];
		}
	}

	void colour(uint32_t index, uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha)
	{
		red = 255;
		green = 255;
		blue = 255;
		alpha = 255;
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
		mTotalTime += frameTime;

		bool updated = mDirty;
		if (mDirty)
		{
			mLines.clear();
			mBounds[0] = glm::vec3(1e10f, 1e10f, 1e10f);
			mBounds[1] = glm::vec3(-1e10f, -1e10f, -1e10f);

			for (size_t i = 0; i < 100; ++i)
			{
				Line line
				{
					{100.0f + i * 4, 100.0f + i * 4},
					{100.0f, 150.0f + sin(mTotalTime + i * 3.14159f * 2.0f / 100.0f) * 100.0f }
				};

				mLines.push_back(line);

				if (line.x[0] < mBounds[0].x)
				{
					mBounds[0].x = line.x[0];
				}
				if (line.y[0] < mBounds[0].y)
				{
					mBounds[0].y = line.y[0];
				}
				if (line.x[1] > mBounds[1].x)
				{
					mBounds[1].x = line.x[1];
				}
				if (line.y[1] > mBounds[1].y)
				{
					mBounds[1].y = line.y[1];
				}
			}

			setNumPrimitives(mLines.size());
			//mDirty = false;
		}

		return mDirty;
	}
};
