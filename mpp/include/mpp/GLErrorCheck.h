#pragma once

#include "mpp/Config.h"

#include <GL/gl.h>

namespace mpp
{
	void CheckOpenGLError(char const* stmt, int line, char const* file, char const* function);
}

#ifdef _DEBUG
#define GL_CHECK(stmt) do { \
            stmt; \
            CheckOpenGLError(#stmt, __LINE__, __FILE__, __func__); \
        } while (0)
#else
#define GL_CHECK(stmt) stmt
#endif