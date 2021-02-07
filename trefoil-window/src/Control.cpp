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

void Control::render(bool hovered)
{
	/*
	auto value = mGetter();
	Vector2 target;
	glColor3fv(mColour);

	if (mOrientation != Orientation::Free)
	{
		glBegin(GL_LINES);
		switch (mOrientation)
		{
		case Orientation::Horizontal:
			target.set(mPosition.x + value.x, mPosition.y);
			break;

		case Orientation::Vertical:
			target.set(mPosition.x, mPosition.y + value.x);
			break;
		}

		glVertex2f(mPosition.x, mPosition.y);
		glVertex2f(target.x, target.y);
		glEnd();
	}
	else
	{
		target = mPosition + value;
	}

	const size_t fanVerts = mOrientation == Orientation::Free ? 9 : 36;

	glBegin(GL_TRIANGLE_FAN);
	glVertex2f(target.x, target.y);
	for (size_t i = 0; i < fanVerts; ++i)
	{
		float angle = i * 360.0f / (float)(fanVerts - 1);
		float radius = (float)(mOrientation == Orientation::Free ? (i % 2 ? 4 : 7) : 7);

		Vector2 v(0, radius);
		v.rotateClockwise(angle);
		v += target;
		
		glVertex2f(v.x, v.y);
	}
	glEnd();

	if (hovered)
	{
		float hcolours[3];
		hcolours[0] = mColour[0] * 1.2f;
		hcolours[1] = mColour[1] * 1.2f;
		hcolours[2] = mColour[2] * 1.2f;

		glColor3fv(hcolours);
		glBegin(GL_LINE_LOOP);
		for (size_t i = 0; i < fanVerts; ++i)
		{
			float angle = i * 360.0f / (float)(fanVerts - 1);
			float radius = (float)(mOrientation == Orientation::Free ? (i % 2 ? 6 : 9) : 9);

			Vector2 v(0, radius);
			v.rotateClockwise(angle);
			v += target;

			glVertex2f(v.x, v.y);
		}
		glEnd();
	}

	if (mShowName)
	{
		switch (mOrientation)
		{
		case Orientation::Horizontal:
			gRenderer->renderString((int)mPosition.x, (int)mPosition.y + 16, mName);
			break;

		case Orientation::Vertical:
			gRenderer->renderString((int)mPosition.x + 16, (int)mPosition.y, mName);
			break;
		}
	}

	if (mShowValue)
	{
		ostringstream out;
		
		out.precision(1);

		float v = mGetter().x;
		v /= 24.0f; // Convert to cm
		out << std::fixed << v;
		string val = out.str();

		switch (mOrientation)
		{
		case Orientation::Horizontal:
			gRenderer->renderString((int)(mPosition.x + value.x + 10), (int)mPosition.y, val);
			break;

		case Orientation::Vertical:
			gRenderer->renderString((int)(mPosition.x + 10), (int)(mPosition.y + value.x), val);
			break;
		}
	}
	*/
}