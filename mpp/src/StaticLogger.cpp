#include <mutex>
#include <fstream>
#include <iomanip>
#include <time.h>

#include "utils/FileSystem.h"

#include "mpp/StaticLogger.h"
#include "mpp/MppException.h"

#define LOG_FILENAME "mpp-resources.log"

namespace mpp
{
	using namespace std;

	static bool gsInitialised = false;

	void _initialise_static_logger()
	{
		// Delete file if it exists
		utils::FileSystem::deleteFile(LOG_FILENAME);
	}

	void _static_log_message(string const& message)
	{
		std::ofstream logger;
		
		logger.open(LOG_FILENAME, ofstream::out | ofstream::app);

		if (!logger.is_open())
		{
			THROW_MPP("Could not open " LOG_FILENAME " for logging.", __LINE__, __FILE__, __func__);
		}

		struct tm *pTime;
		time_t ctTime; time(&ctTime);

#pragma warning(suppress: 4996)
		pTime = localtime(&ctTime);

		logger << setw(2) << setfill('0') << pTime->tm_hour
			<< ":" << setw(2) << setfill('0') << pTime->tm_min
			<< ":" << setw(2) << setfill('0') << pTime->tm_sec
			<< " " << message << "\n";

		logger.close();
	}

	static bool gsEnabled = false;

	void enable_static_log(bool enabled)
	{
		gsEnabled = enabled;
	}

	bool is_static_log_enabled()
	{
		return gsEnabled;
	}

	static std::mutex gMutex;

	void static_log_message(string const& message)
	{
		const lock_guard<std::mutex> lock(gMutex);

		if (gsEnabled)
		{
			if (!gsInitialised)
			{
				_initialise_static_logger();
				gsInitialised = true;
			}

			_static_log_message(message);
		}
	}
}