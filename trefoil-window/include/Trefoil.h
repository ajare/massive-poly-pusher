#pragma once

#include <fstream>
#include <functional>

#include "Vector2.h"

struct Trefoil
{

	float distance, radius;

	size_t numFoils{ 3 };

	float foilOffset{ 0 };

	float getWidth() const
	{
		return 2 * (radius + distance * sin(WP_DEGTORAD(60.0f)));
	}

	float getHeight() const
	{
		return 2 * (radius + distance * sin(WP_DEGTORAD(60.0f)));
	}

	void save(std::ofstream& fp)
	{
		fp.write((char const*)&distance, sizeof(distance));
		fp.write((char const*)&radius, sizeof(radius));
		fp.write((char const*)&numFoils, sizeof(numFoils));
		fp.write((char const*)&foilOffset, sizeof(foilOffset));
	}

	void load(std::ifstream& fp)
	{
		fp.read((char*)&distance, sizeof(distance));
		fp.read((char*)&radius, sizeof(radius));
		fp.read((char*)&numFoils, sizeof(numFoils));
		fp.read((char*)&foilOffset, sizeof(foilOffset));
	}
};
