#pragma once

#include <string>

#include "mpp/mesh/Vertex.h"

#include "Config.h"
#include "glslTypes.h"

namespace mpp
{
	namespace program
	{

		enum class AttributeType
		{
			Position,
			Normal,
			TexCoords,
			Colour,
			UserDefined
		};

		struct _MPPPROGRAMAPI Attribute
		{
			std::string name;
			GLSLTypeDecl type;
			mpp::mesh::Vertex::Component component;
			mpp::mesh::Vertex::DataType dataType;
			bool normalised{ false };

		public:

			std::string getGlslType() const;
		};

	}
}