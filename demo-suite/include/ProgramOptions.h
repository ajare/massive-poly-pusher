#pragma once

#include <string>

struct ProgramOptions
{
	// Video
	int screenWidth{ 0 }, screenHeight{ 0 };
	bool fullScreen{ false }, vSync{ true };

	// Resources
	std::string resourceLocation;
};

ProgramOptions parseProgramOptions(std::string const& filename);