#pragma once

#include <string>

#include <mpp/mesh/Vertex.h>

namespace mpp
{
	namespace program
	{

		enum class GLSLType
		{
			Unknown,
			Bool,
			Int,
			Uint,
			Float,
			Double,
			FloatMatrix,
			DoubleMatrix,
			User
		};

		struct GLSLTypeDecl
		{
			GLSLType type;
			size_t size[2];
			bool isFloatingPoint;
			bool isSigned;
		};

		float getRealComponentIndexDefault(std::string const& component, int index, float def);

		int getSignedComponentIndexDefault(std::string const& component, int index, int def);

		unsigned int getUnsignedComponentIndexDefault(std::string const& component, int index, unsigned int def);
	}
}