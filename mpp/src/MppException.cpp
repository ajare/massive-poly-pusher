#include "mpp/MppException.h"

namespace mpp
{
	using namespace std;

	MppException::MppException(string const& msg, string const& trace)
		: exception(msg.c_str())
		, mTrace(trace)
	{
	}

	MppException::MppException(string const& msg, int line, string const& file, string const& function, string const& trace)
		: exception(msg.c_str())
		, mLine(line)
		, mFile(file)
		, mFunction(function)
		, mTrace(trace)
	{
	}

	int MppException::getLine() const
	{
		return mLine;
	}

	string const& MppException::getFile() const
	{
		return mFile;
	}

	string const& MppException::getFunction() const
	{
		return mFunction;
	}

	string const& MppException::getStackTrace() const
	{
		return mTrace;
	}

	MppIoException::MppIoException(string const& msg)
		: MppException(msg)
	{
	}

	MppIoException::MppIoException(string const& msg, int line, string const& file, string const& function)
		: MppException(msg, line, file, function)
	{
	}

	MppNotImplementedException::MppNotImplementedException(string const& item, int line, string const& file, string const& function)
		: MppException("Not yet implemented: " + item, line, file, function)
	{
	}

	MppNotImplementedException::MppNotImplementedException(int line, string const& file, string const& function)
		: MppException("Not yet implemented: function " + function, line, file, function)
	{
	}

}