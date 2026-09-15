#pragma once

#include <string>

#include "mpp/Config.h"

namespace mpp
{
	// CPU-only tests for the outstanding-reference report that Batch's
	// destruction diagnostic and ResourceManager's redeclaration error share.
	// They construct a Resource with no GPU side, so no OpenGL context is needed.
	_MPPAPI bool runResourceLifetimeTests(std::string* failure = nullptr);
}
