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
				Colour,
				UserDefined
			};

			struct Attribute
			{
				mpp::mesh::Vertex::Component component;
				mpp::mesh::Vertex::DataType dataType;
				bool normalised;
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

				std::vector<Attribute> inAttribs, outAttribs;
			};

		private:

			std::string mName;

			ShaderStage mStages[(int)ShaderStage::Type::NumStages];

			std::map<std::string, AttributeType> mStandardAttributes;

			mesh::MeshSpecification mSpecification;

			std::vector<std::string> mErrors;

			std::vector<std::string> mWarnings;

		private:

			void getInAttributes(ShaderStage& stage);

			void getOutAttributes(ShaderStage& stage);

			void parseAttributeUsage(ShaderStage::Type stageType);

			std::string stripComments(std::string const& src);

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