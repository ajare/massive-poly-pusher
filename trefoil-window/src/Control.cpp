#include <sstream>
#include <algorithm>

#include "Control.h"
#include "TrefoilWindow.h"

using namespace std;

Control::Control(string const& name, Orientation orientation, Getter getter, Setter setter, PositionSetter positionSetter, Getter minValue, Getter maxValue, int windowHeight)
	: mName(name)
	, mPosition(Vector2::ZERO)
	, mGetter(getter)
	, mSetter(setter)
	, mPositionSetter(positionSetter)
	, mMinValue(minValue)
	, mMaxValue(maxValue)
	, mOrientation(orientation)
	, mShowName(false)
	, mShowValue(false)
	, mWindowHeight(windowHeight)
	, mLabelOffset(0)
{
	setColour(1, 1, 1);
}

void Control::showName(bool show)
{
	mShowName = show;
}

void Control::showValue(bool show)
{
	mShowValue = show;
}

void Control::setColour(float r, float g, float b)
{
	mColour[0] = r;
	mColour[1] = g;
	mColour[2] = b;
}

string const& Control::getName() const
{
	return mName;
}

Vector2 Control::getValue() const
{
	return mGetter();
}

bool Control::isHovered(Vector2 const& viewOffset, Vector2 const& position)
{
	auto value = mGetter();
	Vector2 target;

	switch (mOrientation)
	{
	case Orientation::Horizontal:
		target.set(mPosition.x + value.x, mPosition.y);
		break;

	case Orientation::Vertical:
		target.set(mPosition.x, mPosition.y + value.x);
		break;

	case Orientation::Free:
		target = mPosition + value;
		break;
	}

	auto p = position - viewOffset;
	target.y = -target.y;
	return target.distanceTo(p) < 7;
}

Vector2 const& Control::getPosition() const
{
	return mPosition;
}

void Control::setPosition(TrefoilWindow const* window)
{
	mPosition = mPositionSetter(window);
}

void Control::setLabelOffset(float offset)
{
	mLabelOffset = offset;
}

Vector2 Control::getLabelPosition() const
{
	auto pos = mPosition;
	switch (mOrientation)
	{
	case Orientation::Horizontal:
		pos.x += mLabelOffset;
		break;

	case Orientation::Vertical:
		pos.y += mLabelOffset;
		break;
	}

	return pos;
}

Control::Orientation Control::getOrientation() const
{
	return mOrientation;
}

void Control::update(Vector2 const& value)
{
	auto v = mGetter();

	switch (mOrientation)
	{
	case Orientation::Horizontal:
		v.x += value.x;
		break;

	case Orientation::Vertical:
		v.x += value.y;
		break;

	case Orientation::Free:
		v += value;
		break;
	}

	// Bounds
	auto minValue = mMinValue();
	auto maxValue = mMaxValue();

	v.x = min(max(minValue.x, v.x), maxValue.x);
	v.y = min(max(minValue.y, v.y), maxValue.y);
	//v.y = mWindowHeight - v.y;

	mSetter(v);
}
