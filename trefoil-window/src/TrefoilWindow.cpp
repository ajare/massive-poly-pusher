#include <fstream>

#include "TrefoilWindow.h"

using namespace std;

TrefoilWindow::TrefoilWindow(size_t numPanes)
	: mNumPanes(numPanes)
	, mPaneUpperBufferHeight(10)
	, mPaneBaseOffset(10)
	, mPaneSideOffset(10)
	, mPaneWidth(40)
	, mPaneHeight(100)
	, mPaneSpacing(10)
	, mPaneTrefoilOffset(0)
	, mBorderPeak(10)
	, mBorderShoulderOffset(0)
	, mFrameThickness(10)
{
	mControls[0] = Vector2::ZERO;
	mControls[1] = Vector2::ZERO;

	resetTrefoilControls();
}

void TrefoilWindow::setPaneUpperBufferHeight(float height)
{
	mPaneUpperBufferHeight = height;
}

float TrefoilWindow::getPaneUpperBufferHeight() const
{
	return mPaneUpperBufferHeight;
}

void TrefoilWindow::setPaneBaseOffset(float size)
{
	mPaneBaseOffset = size;
}

float TrefoilWindow::getPaneBaseOffset() const
{
	return mPaneBaseOffset;
}

void TrefoilWindow::setPaneSideOffset(float size)
{
	mPaneSideOffset = size;
}

float TrefoilWindow::getPaneSideOffset() const
{
	return mPaneSideOffset;
}

void TrefoilWindow::setPaneWidth(float width)
{
	mPaneWidth = width;
}

float TrefoilWindow::getPaneWidth() const
{
	return mPaneWidth;
}

void TrefoilWindow::setPaneHeight(float height)
{
	mPaneHeight = height;
}

float TrefoilWindow::getPaneHeight() const
{
	return mPaneHeight;
}

void TrefoilWindow::setPaneSpacing(float spacing)
{
	mPaneSpacing = spacing;
}

float TrefoilWindow::getPaneSpacing() const
{
	return mPaneSpacing;
}

void TrefoilWindow::setBorderPeak(float peak)
{
	mBorderPeak = peak;
}

float TrefoilWindow::getBorderPeak() const
{
	return mBorderPeak;
}

void TrefoilWindow::setBorderShoulderOffset(float offset)
{
	mBorderShoulderOffset = offset;
}

float TrefoilWindow::getBorderShoulderOffset() const
{
	return mBorderShoulderOffset;
}

float TrefoilWindow::getWidth() const
{
	return getPaneSpacing() * (getNumPanes() - 1) + 
		getPaneWidth() * getNumPanes() + 
		getPaneSideOffset() * 2;
}

float TrefoilWindow::getHeight() const
{
	return getBorderPeakBase() + getBorderPeak();
}

float TrefoilWindow::getBorderPeakBase() const
{
	return getUpperTrefoilPosition() + mUpperTrefoil.getHeight() / 2;
}

float TrefoilWindow::getShoulderHeight() const
{
	return getPaneBaseOffset() + getPaneHeight() + getBorderShoulderOffset();
}

float TrefoilWindow::getUpperTrefoilPosition() const
{
	return getPaneBaseOffset() + getPaneHeight() + getPaneUpperBufferHeight();
}

size_t TrefoilWindow::getNumPanes() const
{
	return mNumPanes;
}

Vector2 TrefoilWindow::getControl(uint32_t control) const
{
	return mControls[control];
}

void TrefoilWindow::setControl(uint32_t control, Vector2 const& position)
{
	mControls[control] = position;
}

Vector2 const& TrefoilWindow::getTrefoilControlPosition() const
{
	return mTrefoilControlPosition;
}

void TrefoilWindow::setTrefoilControlPosition(Vector2 const& position)
{
	mTrefoilControlPosition = position;
}

float TrefoilWindow::getTrefoilControlRadius() const
{
	return mTrefoilControlRadius;
}

void TrefoilWindow::setTrefoilControlRadius(float radius)
{
	mTrefoilControlRadius = radius;
}

float TrefoilWindow::getTrefoilControlRotation() const
{
	return mTrefoilControlRotation;
}

void TrefoilWindow::setTrefoilControlRotation(float rotation)
{
	mTrefoilControlRotation = rotation;
}

float TrefoilWindow::getPaneTrefoilOffset() const
{
	return mPaneTrefoilOffset;
}

void TrefoilWindow::setPaneTrefoilOffset(float offset)
{
	mPaneTrefoilOffset = offset;
}


void TrefoilWindow::setFrameThickness(float thickness)
{
	mFrameThickness = thickness;
}

float TrefoilWindow::getFrameThickness() const
{
	return mFrameThickness;
}

vector<Vector2> TrefoilWindow::getArcVertices(float x, float width, float height, float shoulderHeight, float arcScale) const
{
	return vector<Vector2>
	{
		{ x - 10, height },
		{ x, height },
		Vector2(x + width / 4, height - 10) + getControl(0) * arcScale,
		Vector2(x + width / 2 - 10, shoulderHeight + (height - shoulderHeight) * 0.5f) + getControl(1) * arcScale,
		{ x + width / 2, shoulderHeight },
		{ x + width / 2, shoulderHeight - 10 }
	};
}

Trefoil const& TrefoilWindow::getUpperTrefoil() const
{
	return mUpperTrefoil;
}

Trefoil& TrefoilWindow::getUpperTrefoil()
{
	return mUpperTrefoil;
}

Trefoil const& TrefoilWindow::getPaneTrefoil() const
{
	return mPaneTrefoil;
}

Trefoil& TrefoilWindow::getPaneTrefoil()
{
	return mPaneTrefoil;
}

void TrefoilWindow::resetTrefoilControls()
{
	// Trefoil settings
	float offset = 4 * tanf(WP_PI / 8) / 3.0f;
	float dOffset = 1.0f - offset;

	mTrefoilControlPosition.set(1.0f - dOffset * 0.5f, 1.0f - dOffset * 0.5f);
	mTrefoilControlRadius = mTrefoilControlPosition.distanceTo(Vector2(offset, 1.0f));
	mTrefoilControlRotation = 0.0f;
}

void TrefoilWindow::save(string const& filename)
{
	ofstream fp(filename, ios::out | ios::binary);

	if (!fp.is_open())
	{
		string errMsg = "Cannot open " + filename + " for writing.";
		throw exception(errMsg.c_str());
	}

	mUpperTrefoil.save(fp);
	mPaneTrefoil.save(fp);

	fp.write((char const*)&mNumPanes, sizeof(mNumPanes));
	fp.write((char const*)&mPaneUpperBufferHeight, sizeof(mPaneUpperBufferHeight));
	fp.write((char const*)&mPaneBaseOffset, sizeof(mPaneBaseOffset));
	fp.write((char const*)&mPaneSideOffset, sizeof(mPaneSideOffset));
	fp.write((char const*)&mPaneWidth, sizeof(mPaneWidth));
	fp.write((char const*)&mPaneHeight, sizeof(mPaneHeight));
	fp.write((char const*)&mPaneSpacing, sizeof(mPaneSpacing));
	fp.write((char const*)&mPaneTrefoilOffset, sizeof(mPaneTrefoilOffset));
	fp.write((char const*)&mBorderPeak, sizeof(mBorderPeak));
	fp.write((char const*)&mBorderShoulderOffset, sizeof(mBorderShoulderOffset));
	fp.write((char const*)&mFrameThickness, sizeof(mFrameThickness));
	fp.write((char const*)&mControls[0].x, sizeof(float));
	fp.write((char const*)&mControls[0].y, sizeof(float));
	fp.write((char const*)&mControls[1].x, sizeof(float));
	fp.write((char const*)&mControls[1].y, sizeof(float));
	fp.write((char const*)&mTrefoilControlPosition.x, sizeof(float));
	fp.write((char const*)&mTrefoilControlPosition.y, sizeof(float));
	fp.write((char const*)&mTrefoilControlRadius, sizeof(mTrefoilControlRadius));
	fp.write((char const*)&mTrefoilControlRotation, sizeof(mTrefoilControlRotation));

	fp.close();
}

void TrefoilWindow::load(string const& filename)
{
	ifstream fp(filename, ios::in | ios::binary);

	if (!fp.is_open())
	{
		string errMsg = "Cannot open " + filename + " for reading.";
		throw exception(errMsg.c_str());
	}

	mUpperTrefoil.load(fp);
	mPaneTrefoil.load(fp);

	fp.read((char*)&mNumPanes, sizeof(mNumPanes));
	fp.read((char*)&mPaneUpperBufferHeight, sizeof(mPaneUpperBufferHeight));
	fp.read((char*)&mPaneBaseOffset, sizeof(mPaneBaseOffset));
	fp.read((char*)&mPaneSideOffset, sizeof(mPaneSideOffset));
	fp.read((char*)&mPaneWidth, sizeof(mPaneWidth));
	fp.read((char*)&mPaneHeight, sizeof(mPaneHeight));
	fp.read((char*)&mPaneSpacing, sizeof(mPaneSpacing));
	fp.read((char*)&mPaneTrefoilOffset, sizeof(mPaneTrefoilOffset));
	fp.read((char*)&mBorderPeak, sizeof(mBorderPeak));
	fp.read((char*)&mBorderShoulderOffset, sizeof(mBorderShoulderOffset));
	fp.read((char*)&mFrameThickness, sizeof(mFrameThickness));
	fp.read((char*)&mControls[0].x, sizeof(float));
	fp.read((char*)&mControls[0].y, sizeof(float));
	fp.read((char*)&mControls[1].x, sizeof(float));
	fp.read((char*)&mControls[1].y, sizeof(float));
	fp.read((char*)&mTrefoilControlPosition.x, sizeof(float));
	fp.read((char*)&mTrefoilControlPosition.y, sizeof(float));
	fp.read((char*)&mTrefoilControlRadius, sizeof(mTrefoilControlRadius));
	fp.read((char*)&mTrefoilControlRotation, sizeof(mTrefoilControlRotation));

	fp.close();
}