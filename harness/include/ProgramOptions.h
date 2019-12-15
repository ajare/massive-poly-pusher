#pragma once

#include <string>

struct ProgramOptions
{
	// Video
	int screenWidth, screenHeight;
	bool fullScreen, vSync;

	// Resources
	std::string resourceLocation;
};

ProgramOptions parseProgramOptions(std::string const& filename);