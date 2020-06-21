#pragma once

#include <string>

#include "glslTypes.h"

namespace mpp
{
	namespace program
	{

		struct Uniform
		{
			std::string name;
			GLSLTypeDecl type;
		};

	}
}