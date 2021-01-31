#pragma once

#include <string>
#include <functional>

#include "Vector2.h"

class TrefoilWindow;

class Control
{
public:

	typedef std::function<void(Vector2 const&)> Setter;

	typedef std::function<Vector2()> Getter;

	typedef std::function<Vector2(TrefoilWindow const*)> PositionSetter;

	enum class Orientation
	{
		Horizontal,
		Vertical,
		Free
	};

private:

	std::string mName;

	Vector2 mPosition;

	Getter mGetter;

	Setter mSetter;

	Getter mMinValue, mMaxValue;

	PositionSetter mPositionSetter;

	Orientation mOrientation;

	float mColour[3];

	bool mShowName;

	bool mShowValue;

	int mWindowHeight;

public:

	Control(std::string const& name, Orientation orientation, Getter getter, Setter setter, PositionSetter positionSetter, Getter minValue, Getter maxValue, int windowHeight);

	void showName(bool show);

	void showValue(bool show);

	void setColour(float r, float g, float b);

	std::string const& getName() const;

	Vector2 getValue() const;

	bool isHovered(Vector2 const& viewOffset, Vector2 const& position);

	Vector2 const& getPosition() const;

	void setPosition(TrefoilWindow const* window);

	Orientation getOrientation() const;

	void update(Vector2 const& value);
	
	void render(bool hovered);
};
