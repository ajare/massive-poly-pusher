#pragma once

#include <Windows.h>
#include <gl/GL.h>
#include <exception>
#include <string>

#include "mpp/Config.h"
#include "mpp/DebugStackWalker.h"

#pragma warning(push)
#pragma warning(disable : 4275)

namespace mpp
{
	class _MPPAPI MppException : public std::exception
	{
		int mLine{ 0 };

		std::string mFile;

		std::string mFunction;

		std::string mTrace;

	public:

		explicit MppException(std::string const& msg, std::string const& trace = "not available");

		MppException(std::string const& msg, int line, std::string const& file, std::string const& function, std::string const& trace = "not available");

		int getLine() const;

		std::string const& getFile() const;

		std::string const& getFunction() const;

		std::string const& getStackTrace() const;
	};

	class _MPPAPI MppIoException : public MppException
	{
	public:

		explicit MppIoException(std::string const& msg);

		MppIoException(std::string const& msg, int line, std::string const& file, std::string const& function);
	};

	class _MPPAPI MppGlException : public MppException
	{
		GLenum mErrorCode;

	private:

		std::string getErrorMessage(GLenum errorCode);

	public:

		explicit MppGlException(GLenum errorCode);

		MppGlException(GLenum errorCode, int line, std::string const& file, std::string const& function);

		GLenum getErrorCode() const;
	};

	class _MPPAPI MppNotImplementedException : public MppException
	{
	public:

		MppNotImplementedException(std::string const& item, int line, std::string const& file, std::string const& function);

		MppNotImplementedException(int line, std::string const& file, std::string const& function);
	};

}

#pragma warning(pop)

#define THROW_MPP(errMsg, line, file, function) \
	do											\
	{											\
		std::string trace;						\
		DebugStackWalker sw(&trace);			\
		sw.ShowCallstack();						\
		throw mpp::MppException(errMsg, line, file, function, trace); \
	} while (false)

#define THROW_MPP_IO(errMsg, line, file, function) throw mpp::MppIoException(errMsg, line, file, function)
#define THROW_MPP_GL(errCode, line, file, function) throw mpp::MppGlException(errCode, line, file, function)
#define THROW_MPP_NOTIMP(item, line, file, function) throw mpp::MppNotImplementedException(item, line, file, function)
#define THROW_MPP_FN_NOTIMP(line, file, function) throw mpp::MppNotImplementedException(line, file, function)