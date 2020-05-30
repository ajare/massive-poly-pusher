#pragma once

#include <Windows.h>
#include <gl/GL.h>

namespace mpp
{
	void CheckOpenGLError(char const* stmt, int line, char const* file, char const* function);
}

#ifdef _DEBUG
#define GL_CHECK(stmt) do { \
            stmt; \
            CheckOpenGLError(#stmt, __LINE__, __FILE__, __FUNCTION__); \
        } while (0)
#else
#define GL_CHECK(stmt) stmt
#endif