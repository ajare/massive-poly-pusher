#pragma once

#include <vector>

#include "Trefoil.h"

class TrefoilWindow
{
	Trefoil mUpperTrefoil;

	Trefoil mPaneTrefoil;

	size_t mNumPanes;

	float mPaneUpperBufferHeight;

	float mPaneBaseOffset;

	float mPaneSideOffset;

	float mPaneWidth;

	float mPaneHeight;

	float mPaneSpacing;

	float mPaneTrefoilOffset;

	float mBorderPeak;

	float mBorderShoulderOffset;

	float mFrameThickness;

	Vector2 mControls[2];

	// Trefoil settings.  We have a point which we use
	// to generate the control points.  This point has
	// a position, a radius, and a rotation
	float mTrefoilControlRadius, mTrefoilControlRotation;

public:

	explicit TrefoilWindow(size_t numPanes);

	void setPaneUpperBufferHeight(float height);

	float getPaneUpperBufferHeight() const;

	void setPaneBaseOffset(float offset);

	float getPaneBaseOffset() const;

	void setPaneSideOffset(float offset);

	float getPaneSideOffset() const;

	void setPaneWidth(float width);

	float getPaneWidth() const;

	void setPaneHeight(float height);

	float getPaneHeight() const;

	void setPaneSpacing(float spacing);

	float getPaneSpacing() const;

	void setBorderPeak(float peak);

	float getBorderPeak() const;

	void setBorderShoulderOffset(float offset);

	float getBorderShoulderOffset() const;

	float getWidth() const;

	float getHeight() const;

	float getBorderPeakBase() const;

	float getShoulderHeight() const;

	float getUpperTrefoilPosition() const;

	size_t getNumPanes() const;

	Vector2 getControl(uint32_t control) const;

	void setControl(uint32_t control, Vector2 const& position);

	float getPaneTrefoilOffset() const;

	void setPaneTrefoilOffset(float offset);

	void setFrameThickness(float thickness);

	float getFrameThickness() const;

	std::vector<Vector2> getArcVertices(float x, float width, float height, float shoulderHeight, float arcScale) const;

	Trefoil const& getUpperTrefoil() const;

	Trefoil& getUpperTrefoil();

	Trefoil const& getPaneTrefoil() const;

	Trefoil& getPaneTrefoil();

	void resetTrefoilControls();

	void save(std::string const& filename);

	void load(std::string const& filename);
};
