#include "mpp/MppException.h"

namespace mpp
{
	using namespace std;

	MppException::MppException(string const& msg, string const& trace)
		: runtime_error(msg)
		, mTrace(trace)
	{
	}

	MppException::MppException(string const& msg, int line, string const& file, string const& function, string const& trace)
		: runtime_error(msg)
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

	string MppGlException::getErrorMessage(GLenum errorCode)
	{
		switch (errorCode)
		{
		case GL_INVALID_ENUM:
			return "GL_INVALID_ENUM";

		case GL_INVALID_VALUE:
			return "GL_INVALID_VALUE";

		case GL_INVALID_OPERATION:
			return "GL_INVALID_OPERATION";

		case GL_STACK_OVERFLOW:
			return "GL_STACK_OVERFLOW";

		case GL_STACK_UNDERFLOW:
			return "GL_STACK_UNDERFLOW";

		case GL_OUT_OF_MEMORY:
			return "GL_OUT_OF_MEMORY";

		default:
			return "GL: unknown error";
		}
	}

	MppGlException::MppGlException(GLenum errorCode)
		: MppException(getErrorMessage(errorCode))
		, mErrorCode(errorCode)
	{
	}

	MppGlException::MppGlException(GLenum errorCode, int line, string const& file, string const& function)
		: MppException(getErrorMessage(errorCode), line, file, function)
		, mErrorCode(errorCode)
	{
	}

	GLenum MppGlException::getErrorCode() const
	{
		return mErrorCode;
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