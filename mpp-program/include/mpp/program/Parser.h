#pragma once

#include <string>
#include <set>
#include <algorithm>

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
				std::string name;
				mpp::mesh::Vertex::Component component;
				mpp::mesh::Vertex::DataType dataType;
				bool normalised{ false };
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

			public:

				bool required() const { return type == Type::Vertex || type == Type::Fragment; }

				bool provided() const { return source != ""; }

				bool inAttributeExists(std::string const& attrib) const
				{
					return std::find_if(inAttribs.begin(), inAttribs.end(), [attrib](auto const& attrStruct)
					{
						return attrStruct.name == attrib;
					}) != inAttribs.end();
				}

				bool outAttributeExists(std::string const& attrib) const
				{
					return std::find_if(outAttribs.begin(), outAttribs.end(), [attrib](auto const& attrStruct)
					{
						return attrStruct.name == attrib;
					}) != outAttribs.end();
				}
			};

		private:

			std::string mName;

			ShaderStage mStages[(int)ShaderStage::Type::NumStages];

			std::map<std::string, AttributeType> mStandardAttributes;

			mesh::MeshSpecification mSpecification;

			std::vector<std::string> mErrors;

			std::vector<std::string> mWarnings;

		private:

			void setInAttributesToMeshSpecification(ShaderStage::Type stageType);

			void setInAttributesToPreviousStage(ShaderStage::Type stageType);

			void setOutAttributesToUsage(ShaderStage::Type stageType);

			void parseInAttributeUsage(ShaderStage::Type stageType);

			std::string generateShader(ShaderStage::Type stageType);

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