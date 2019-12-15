#pragma once

#include <fstream>
#include <string>

#include "mpp/Config.h"

namespace mpp
{
	class _MPPAPI Logger
	{
		std::ofstream mLog;

		std::string mFileName;

	public:

		~Logger();

		bool initialise(std::string const& fileName);

		void message(std::string const& msg);
	};

}