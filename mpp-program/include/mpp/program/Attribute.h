#pragma once

#include <string>

#include "mpp/mesh/Vertex.h"

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

		struct Attribute
		{
			std::string name;
			mpp::mesh::Vertex::Component component;
			mpp::mesh::Vertex::DataType dataType;
			bool normalised{ false };

		public:

			std::string getGlslType() const;
		};

	}
}