#pragma once

#include <fstream>
#include <string>

class Logger
{
	std::ofstream mLog;

	std::string mFileName;

public:

	~Logger();

	bool initialise(std::string const& fileName);

	void message(std::string const& msg);
};
