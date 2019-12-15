#pragma once

#include "mpp/Config.h"

namespace mpp
{
	struct _MPPAPI ClipRectangle
	{
		int x, y, width, height;

	public:

		ClipRectangle();

		ClipRectangle(int _x, int _y, int _width, int _height);

		ClipRectangle intersect(ClipRectangle const& clipper) const;
	};
}
