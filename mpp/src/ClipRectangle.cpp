#include <algorithm>

#include "mpp/ClipRectangle.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{

	/*
	 * Default constructor.
	 *
	 */
	ClipRectangle::ClipRectangle()
		: x(0)
		, y(0)
		, width(0)
		, height(0)
	{
	}

	/*
	 * Constructor.
	 *
	 */
	ClipRectangle::ClipRectangle (int _x, int _y, int _width, int _height)
		: x(_x)
		, y(_y)
		, width(_width)
		, height(_height)	
	{
	}

	/*
	 * Clip the rectangle against another, producing a refined clipping area.
	 *
	 */
	ClipRectangle ClipRectangle::intersect(ClipRectangle const& clipper) const
	{
		THROW_MPP_FN_NOTIMP(__LINE__, __FILE__, __FUNCTION__);
	}

}