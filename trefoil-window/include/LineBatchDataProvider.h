#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>

#include <mpp/helper/LineBatchDataProvider.h>

#include "TrefoilWindow.h"
#include "Control.h"
#include "Generation.h"

class TrefoilWindowDataProvider	: public mpp::helper::LineBatchDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	TrefoilWindow* mWindow;

	std::vector<Vector2> mLines;

	bool mDirty{ true };

public:

	TrefoilWindowDataProvider(mpp::RenderSystem* renderSystem, TrefoilWindow* window)
		: mRenderSystem(renderSystem)
		, mWindow(window)
	{
		update(0.0f);
	}

	void getBounds(glm::vec3& bMin, glm::vec3& bMax) override
	{
		bMin.x = bMin.y = bMin.z = -1e10f;
		bMax.x = bMax.y = bMax.z = 1e10f;
	}

	void position(uint32_t index, float& x0, float& y0, float& x1, float& y1)
	{
		if (index < getNumPrimitives())
		{
			x0 = mLines[index * 2 + 0].x;
			y0 = mLines[index * 2 + 0].y;
			x1 = mLines[index * 2 + 1].x;
			y1 = mLines[index * 2 + 1].y;
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
			// Regen if window changed
			auto lines = generateLines(mWindow);

			// Flatten
			mLines.clear();
			for (auto const& loop : lines)
			{
				for (size_t i = 0; i < loop.size() - 1; ++i)
				{
					mLines.push_back(loop[i]);
					mLines.push_back(loop[i + 1]);
				}

				mLines.push_back(loop.back());
				mLines.push_back(loop.front());
			}

			setNumPrimitives(mLines.size() / 2);
			mDirty = false;
		}
	}
};

class ControlLinesDataProvider : public mpp::helper::LineBatchDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	std::vector<Control*> mControls;

	std::vector<Vector2> mLines;

	bool mDirty{ true };

public:

	ControlLinesDataProvider(mpp::RenderSystem* renderSystem, std::vector<Control*> controls)
		: mRenderSystem(renderSystem)
		, mControls(controls)
	{
		setNumPrimitives(0);
	}

	void getBounds(glm::vec3& bMin, glm::vec3& bMax) override
	{
		bMin.x = bMin.y = bMin.z = -1e10f;
		bMax.x = bMax.y = bMax.z = 1e10f;
	}

	void position(uint32_t index, float& x0, float& y0, float& x1, float& y1)
	{
		if (index < getNumPrimitives())
		{
			x0 = mLines[index * 2 + 0].x;
			y0 = mLines[index * 2 + 0].y;
			x1 = mLines[index * 2 + 1].x;
			y1 = mLines[index * 2 + 1].y;
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

			// Regen if controls changed
			for (auto control: mControls)
			{
				auto value = control->getValue();
				auto const& pos = control->getPosition();

				switch (control->getOrientation())
				{
				case Control::Orientation::Horizontal:
					mLines.push_back(pos);
					mLines.push_back(Vector2(pos.x + value.x, pos.y));
					break;

				case Control::Orientation::Vertical:
					mLines.push_back(pos);
					mLines.push_back(Vector2(pos.x, pos.y + value.x));
					break;
				}
			}

			setNumPrimitives(mLines.size() / 2);
			mDirty = false;
		}
	}
};
