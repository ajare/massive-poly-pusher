#include <iomanip>
#include <time.h>
#include "Logger.h"

using namespace std;

// --------------------------------------------------------------------------------
Logger::~Logger()
{
	if (mLog.is_open())
		mLog.close();
}
// --------------------------------------------------------------------------------
bool Logger::initialise(string const& fileName)
{
	mFileName = fileName;

	mLog.open(fileName.c_str());
	
	if (!mLog.is_open())
	{
		// No point adding an error message!
		return false;
	}

	return true;
}
// --------------------------------------------------------------------------------
void Logger::message(string const& msg)
{
	if (!mLog.is_open())
		return;
	
	struct tm *pTime;
	time_t ctTime; time (&ctTime);

#pragma warning(push)
#pragma warning(disable: 4996)
	pTime = localtime (&ctTime);
#pragma warning(pop)

	mLog << setw(2) << setfill('0') << pTime->tm_hour
		<< ":" << setw(2) << setfill('0') << pTime->tm_min
		<< ":" << setw(2) << setfill('0') << pTime->tm_sec
		<< " " << msg << endl;

	// Flush stream to ensure it is written (in case of a crash)
	mLog.flush();
}
// --------------------------------------------------------------------------------
