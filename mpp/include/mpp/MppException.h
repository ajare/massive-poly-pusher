#pragma once

#include <Windows.h>
#include <gl/GL.h>
#include <exception>
#include <string>

namespace mpp
{
	class MppException : public std::exception
	{
		int mLine{ 0 };

		std::string mFile;

		std::string mFunction;

	public:

		explicit MppException(std::string const& msg)
			: std::exception(msg.c_str())
		{
		}

		MppException(std::string const& msg, int line, std::string const& file, std::string const& function)
			: std::exception(msg.c_str())
			, mLine(line)
			, mFile(file)
			, mFunction(function)
		{
		}

		int getLine() const
		{
			return mLine;
		}

		std::string const& getFile() const
		{
			return mFile;
		}

		std::string const& getFunction() const
		{
			return mFunction;
		}
	};

	class MppIoException : public MppException
	{
	public:

		explicit MppIoException(std::string const& msg)
			: MppException(msg)
		{
		}

		MppIoException(std::string const& msg, int line, std::string const& file, std::string const& function)
			: MppException(msg, line, file, function)
		{
		}
	};

	class MppGlException : public MppException
	{
		GLenum mErrorCode;

	private:

		std::string getErrorMessage(GLenum errorCode)
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

	public:

		explicit MppGlException(GLenum errorCode)
			: MppException(getErrorMessage(errorCode))
			, mErrorCode(errorCode)
		{
		}

		MppGlException(GLenum errorCode, int line, std::string const& file, std::string const& function)
			: MppException(getErrorMessage(errorCode), line, file, function)
			, mErrorCode(errorCode)
		{
		}

		GLenum getErrorCode() const
		{
			return mErrorCode;
		}
	};

	class MppNotImplementedException : public MppException
	{
	public:

		MppNotImplementedException(std::string const& item, int line, std::string const& file, std::string const& function)
			: MppException("Not yet implemented: " + item, line, file, function)
		{
		}

		MppNotImplementedException(int line, std::string const& file, std::string const& function)
			: MppException("Not yet implemented: function " + function, line, file, function)
		{
		}
	};

}

#define THROW_MPP(errMsg, line, file, function) throw mpp::MppException(errMsg, line, file, function)
#define THROW_MPP_IO(errMsg, line, file, function) throw mpp::MppIoException(errMsg, line, file, function)
#define THROW_MPP_GL(errCode, line, file, function) throw mpp::MppGlException(errCode, line, file, function)
#define THROW_MPP_NOTIMP(item, line, file, function) throw mpp::MppNotImplementedException(item, line, file, function)
#define THROW_MPP_FN_NOTIMP(line, file, function) throw mpp::MppNotImplementedException(line, file, function)