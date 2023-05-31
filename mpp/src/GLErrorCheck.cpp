#include "mpp/GLErrorCheck.h"
#include "mpp/MppException.h"

namespace mpp
{
	using namespace std;

	string getErrorMessage(GLenum errorCode)
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

	void CheckOpenGLError(char const* stmt, int line, char const* file, char const* function)
	{
		auto err = glGetError();
		if (err != 0)
		{
			THROW_MPP(getErrorMessage(err), line, file, function);
		}
	}
}