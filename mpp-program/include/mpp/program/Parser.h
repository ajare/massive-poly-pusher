#pragma once

#include <string>
#include <set>

#include <mpp/mesh/MeshSpecification.h>

#include "Config.h"

namespace mpp
{
	namespace program
	{

		class _MPPPROGRAMAPI Parser
		{
			enum class AttributeType
			{
				Position,
				Normal,
				TexCoords,
				Colour
			};

		private:

			struct ShaderStage
			{
				enum class Type
				{
					Vertex,
					Geometry,
					Fragment,
					NumStages,
				};

				Type type;
				std::string source;

				std::set<AttributeType> in, out;
			};

		private:

			std::string mName;

			ShaderStage mStages[(int)ShaderStage::Type::NumStages];

			mesh::MeshSpecification mSpecification;

			std::set<AttributeType> mSpecificationAttributes;

			std::vector<std::string> mErrors;

			std::vector<std::string> mWarnings;

		private:

			void parseAttributeUsage(ShaderStage::Type stageType);

			void checkUnusedAttributes(std::set<AttributeType> const& attribs);

		public:

			Parser();

			explicit Parser(std::string const& name);

			std::string const& getName() const;

			void setVertexSource(std::string const& src);

			void setGeometrySource(std::string const& src);

			void setFragmentSource(std::string const& src);

			void setMeshSpecification(mesh::MeshSpecification const& spec);

			void build();

			std::vector<std::string> const& getErrors() const;

			std::vector<std::string> const& getWarnings() const;
		};

	}
}