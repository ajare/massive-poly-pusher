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

	bool mDirty{ true };

public:

	TestLineBatchDataProvider()
	{
		update(0.0f);
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

	void update(float frameTime)
	{
		if (mDirty)
		{
			mLines.clear();
			for (size_t i = 0; i < 100; ++i)
			{
				Line line
				{
					{100.0f + i * 4, 100.0f + i * 4},
					{100.0f, 200.0f}
				};

				mLines.push_back(line);
			}

			setNumPrimitives(mLines.size());
			mDirty = false;
		}
	}
};
