#pragma once

#include <string>
#include <set>
#include <algorithm>

#include <mpp/mesh/MeshSpecification.h>

#include "Config.h"

#define MPP_PROGRAM_VS_IN_PREFIX			"_mpp_vs_in_"
#define MPP_PROGRAM_VS_OUT_PREFIX			"_mpp_vs_out_"

#define MPP_PROGRAM_GS_IN_PREFIX			"_mpp_gs_in_"
#define MPP_PROGRAM_GS_OUT_PREFIX			"_mpp_gs_out_"

#define MPP_PROGRAM_FS_IN_PREFIX			"_mpp_fs_in_"
#define MPP_PROGRAM_FS_OUT_PREFIX			"_mpp_fs_out_"

#define MPP_PROGRAM_MCPMATRIX_TOKEN			"@MCPMatrix"
#define MPP_PROGRAM_NORMALMATRIX_TOKEN		"@NormalMatrix"
#define MPP_PROGRAM_HALFWINDOWSIZE_TOKEN	"@HalfWindowSize"

#define MPP_PROGRAM_UNIFORM_PREFIX			"_mpp_u_"
#define MPP_PROGRAM_TEXTURE_PREFIX			"_mpp_t_"

#define MPP_PROGRAM_MCPMATRIX_NAME			(MPP_PROGRAM_UNIFORM_PREFIX "modelCameraProjection_")
#define MPP_PROGRAM_NORMALMATRIX_NAME		(MPP_PROGRAM_UNIFORM_PREFIX "normal_")
#define MPP_PROGRAM_HALFWINDOWSIZE_NAME		(MPP_PROGRAM_UNIFORM_PREFIX "halfWindowSize_")

#define MPP_PROGRAM_MARKUP_UNIFORM(token)	(MPP_PROGRAM_UNIFORM_PREFIX + token + "_")
#define MPP_PROGRAM_MARKUP_TEXTURE(token)	(MPP_PROGRAM_TEXTURE_PREFIX + token + "_")

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

			struct Uniform
			{
				std::string name;
				std::string type;
			};

			struct Texture
			{
				std::string name;
				std::string type;
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
				std::string generated;
				int mainLine{ -1 };

				std::vector<Attribute> inAttribs, outAttribs;
				std::vector<Uniform> uniforms;
				std::vector<Texture> textures;

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

			void parseSource(ShaderStage::Type stageType);

			void parseUniformUsage(ShaderStage::Type stageType);

			void parseTextureUsage(ShaderStage::Type stageType);

			void setInAttributesToMeshSpecification(ShaderStage::Type stageType);

			void setInAttributesToPreviousStage(ShaderStage::Type stageType);

			void setOutAttributesToUsage(ShaderStage::Type stageType);

			void parseInAttributeUsage(ShaderStage::Type stageType);

			void generateShader(ShaderStage::Type stageType);

			std::string stripComments(std::string const& src);

			std::vector<std::string> splitSourceIntoLines(std::string const& src);

			void addError(ShaderStage::Type stageType, std::string const& error);

			void addWarning(ShaderStage::Type stageType, std::string const& error);

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