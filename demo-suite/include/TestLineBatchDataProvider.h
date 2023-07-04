#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>

#include <mpp/helper/LineBatchDataProvider.h>

#define DEGTORAD(d) ((d) * 3.14159f / 180.0f)

class TestLineBatchDataProvider : public mpp::helper::LineBatchDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>
{
	struct Line
	{
		float x[2], y[2];
		uint8_t c[4];
	};

	std::vector<Line> mLines;

	uint32_t mNumLines;

	float mLinePct;

	glm::vec3 mBounds[2];

	int mX, mY, mWidth, mHeight;

	float mTotalTime;

	bool mDirty{ true };

public:

	TestLineBatchDataProvider(int x, int y, int width, int height)
		: mLinePct(0.8f)
		, mX(x)
		, mY(y)
		, mWidth(width)
		, mHeight(height)
		, mTotalTime(0.0f)
	{
		mNumLines = (uint32_t)(width * mLinePct * 0.4f);
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
			auto const& line = mLines[index];

			x0 = line.x[0];
			y0 = line.y[0];
			x1 = line.x[1];
			y1 = line.y[1];
		}
	}

	void colour(uint32_t index, uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha)
	{
		auto const& line = mLines[index];

		red = line.c[0];
		green = line.c[1];
		blue = line.c[2];
		alpha = line.c[3];
	}

	mpp::Colour diffuse()
	{
		return mpp::Colour::White;
	}

	bool update(float frameTime)
	{
		mTotalTime += frameTime;

		mLines.clear();
		mBounds[0] = glm::vec3(1e10f, 1e10f, 1e10f);
		mBounds[1] = glm::vec3(-1e10f, -1e10f, -1e10f);

		float x = mX + ((1.0f - mLinePct) * mWidth * 0.5f);
		float h2 = mHeight / 2.0f;
		float y = mY + h2;
		float w = mWidth * mLinePct;
		float dx = w / mNumLines;

		for (size_t i = 0; i < mNumLines; ++i)
		{
			uint8_t r, g, b;

			r = 128 + i % 128;
			g = 128 + (i * 3) % 128;
			b = 128 + (i * 7) % 128;

			Line line
			{
				{
					x + dx * i, 
					x + dx * i
				},
				{ 
					y + sin(mTotalTime * 0.7f + DEGTORAD(360 * i * 1.8f / mNumLines)) * h2 * 0.9f,
					y + cos(mTotalTime + DEGTORAD(360 * i / mNumLines)) * h2 * 0.9f 
				},
				{ 
					r, g, b, 255 
				}
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

		return true;
	}
};
