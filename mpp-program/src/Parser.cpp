#include <set>
#include <map>
#include <regex>

#include <utils/StringUtils.h>

#include "mpp/program/Parser.h"
#include "mpp/program/MppProgramException.h"

using namespace std;

namespace mpp
{
	namespace program
	{

		/*
		 * Constructor.
		 *
		 */
		Parser::Parser()
			: Parser("<unnamed program>")
		{
		}

		/*
		 * Constructor.
		 *
		 */
		Parser::Parser(std::string const& name)
			: mName(name)
		{
			mStandardAttributes =
			{
				{"POSITION", AttributeType::Position},
				{"NORMAL", AttributeType::Normal},
				{"TEXCOORDS", AttributeType::TexCoords},
				{"COLOUR", AttributeType::Colour}
			};
		}

		/*
		 * Get program name.
		 *
		 */
		string const& Parser::getName() const
		{
			return mName;
		}

		/*
		 * Set vertex shader source.
		 *
		 */
		void Parser::setVertexSource(string const& src)
		{
			auto& stage = mStages[(int)ShaderStage::Type::Vertex];
			
			stage.type = ShaderStage::Type::Vertex;
			stage.source = stripComments(src);
			stage.inAttribs.clear();
			stage.outAttribs.clear();
		}

		/*
		 * Set geometry shader source.
		 *
		 */
		void Parser::setGeometrySource(string const& src)
		{
			auto& stage = mStages[(int)ShaderStage::Type::Geometry];

			stage.type = ShaderStage::Type::Geometry;
			stage.source = stripComments(src);
			stage.inAttribs.clear();
			stage.outAttribs.clear();
		}

		/*
		 * Set fragment shader source.
		 *
		 */		
		void Parser::setFragmentSource(string const& src)
		{
			auto& stage = mStages[(int)ShaderStage::Type::Fragment];

			stage.type = ShaderStage::Type::Fragment;
			stage.source = stripComments(src);
			stage.inAttribs.clear();
			stage.outAttribs.clear();
		}

		/*
		 * Set mesh specification to build program with.
		 *
		 */
		void Parser::setMeshSpecification(mesh::MeshSpecification const& spec)
		{
			mSpecification = spec;
		}

		/*
		 * Remove comments from GLSL source code.
		 *
		 */
		string Parser::stripComments(string const& src)
		{
			enum class Mode
			{
				Copy,
				Single,
				Multi
			};

			string stripped;
			Mode mode = Mode::Copy;

			for (size_t i = 0; i < src.size(); ++i)
			{
				if (src[i] == '/' && i < (src.size() - 1))
				{
					if (src[i + 1] == '/' && mode == Mode::Copy)
					{
						mode = Mode::Single;
						i += 1;
					}
					else if (src[i + 1] == '*' && mode == Mode::Copy)
					{
						mode = Mode::Multi;
						i += 1;
					}
				}
				else if (src[i] == '\n')
				{
					if (mode == Mode::Single)
					{
						mode = Mode::Copy;
					}
				}
				else if (src[i] == '*' && i < (src.size() - 1))
				{
					if (src[i + 1] == '/' && mode == Mode::Multi)
					{
						mode = Mode::Copy;
						i += 2;
					}
				}

				if (mode == Mode::Copy)
				{
					stripped.push_back(src[i]);
				}
			}

			if (mode != Mode::Copy)
			{
				THROW_MPP_PROGRAM("Invalid comment found in '" + mName + "'.", __LINE__, __FILE__, __FUNCTION__);
			}

			return stripped;
		}

		/*
		 * Set in attributes based on mesh specification.
		 *
		 */
		void Parser::setInAttributesToMeshSpecification(ShaderStage::Type stageType)
		{
			auto& stage = mStages[(int)stageType];

			stage.inAttribs.clear();

			for (int i = 0; i < mSpecification.getNumVertexBufferAttributeLayouts(); ++i)
			{
				auto const& layout = mSpecification.getVertexBufferAttributeLayout(i);
				for (int j = 0; j < layout.getNumAttributes(); ++j)
				{
					auto const& meshAttrib = layout.getAttribute(j);

					Attribute inAttrib{ meshAttrib.component, meshAttrib.dataType, meshAttrib.normalised };
					stage.inAttribs.push_back(inAttrib);
				}
			}
		}

		/*
		 * Set in attributes to previous stage's out attributes.
		 *
		 */
		void Parser::setInAttributesToPreviousStage(ShaderStage::Type stageType)
		{
			auto& stage = mStages[(int)stageType];

			int prevStage = (int)stageType - 1;
			while (mStages[prevStage].source == "")
			{
				prevStage--;
			}

			stage.inAttribs = mStages[prevStage].outAttribs;
		}

		/*
		 * Set out attributes based on how they are used in the shader.
		 *
		 */
		void Parser::setOutAttributesToUsage(ShaderStage::Type stageType)
		{
			auto& stage = mStages[(int)stageType];

			stage.outAttribs.clear();

			regex re(R"(@Out\s*\(\s*([\w\d]+\s+)?([\w\d]+)\s*\))");
			smatch match;

			auto src = stage.source;
			while (regex_search(src, match, re))
			{
				if (match.size() == 2)
				{
					auto attrib = utils::StringUtils::trim(match.str(1));

					// Type not defined: see if it's already been so.
					// TODO: ...
				}
				else
				{
					auto type = utils::StringUtils::trim(match.str(1));
					auto attrib = utils::StringUtils::trim(match.str(2));

					// If type is empty, then there is no type declaration, so check it's
					// already been defined.
					// TODO: ...

					auto it = mStandardAttributes.find(attrib);
					if (it == mStandardAttributes.end())
					{
						mErrors.push_back("Unknown out attribute '" + attrib + "' used.");
						goto next_match;
					}

					auto cType = it->second;

					switch (cType)
					{
					case AttributeType::Position:
						if (type == "vec2")
							stage.outAttribs.push_back({ mpp::mesh::Vertex::Component::Position2, mpp::mesh::Vertex::DataType::Float });
						else if (type == "vec3")
							stage.outAttribs.push_back({ mpp::mesh::Vertex::Component::Position3, mpp::mesh::Vertex::DataType::Float });
						else if (type == "vec4")
							stage.outAttribs.push_back({ mpp::mesh::Vertex::Component::Position4, mpp::mesh::Vertex::DataType::Float });
						else
							mErrors.push_back("Unsupported out attribute type '" + type + "' used for " + attrib);
						break;
					case AttributeType::Normal:
						if (type == "vec3")
							stage.outAttribs.push_back({ mpp::mesh::Vertex::Component::Normal3, mpp::mesh::Vertex::DataType::Float });
						else if (type == "vec4")
							stage.outAttribs.push_back({ mpp::mesh::Vertex::Component::Normal4, mpp::mesh::Vertex::DataType::Float });
						else
							mErrors.push_back("Unsupported out attribute type '" + type + "' used for " + attrib);
						break;
					case AttributeType::TexCoords:
						if (type == "vec2")
							stage.outAttribs.push_back({ mpp::mesh::Vertex::Component::TexCoord2, mpp::mesh::Vertex::DataType::Float });
						else if (type == "vec3")
							stage.outAttribs.push_back({ mpp::mesh::Vertex::Component::TexCoord3, mpp::mesh::Vertex::DataType::Float });
						else if (type == "vec4")
							stage.outAttribs.push_back({ mpp::mesh::Vertex::Component::TexCoord4, mpp::mesh::Vertex::DataType::Float });
						else
							mErrors.push_back("Unsupported out attribute type '" + type + "' used for " + attrib);
						break;
					case AttributeType::Colour:
						if (type == "float")
							stage.outAttribs.push_back({ mpp::mesh::Vertex::Component::Colour1, mpp::mesh::Vertex::DataType::Float });
						else if (type == "vec3")
							stage.outAttribs.push_back({ mpp::mesh::Vertex::Component::Colour3, mpp::mesh::Vertex::DataType::Float });
						else if (type == "vec4")
							stage.outAttribs.push_back({ mpp::mesh::Vertex::Component::Colour4, mpp::mesh::Vertex::DataType::Float });
						else
							mErrors.push_back("Unsupported out attribute type '" + type + "' used for " + attrib);
						break;
					}
				}

				// Look for next match
				next_match: src = match.suffix().str();
			}
		}

		/*
		 * Get attributes used from the source code.
		 *
		 */
		void Parser::parseInAttributeUsage(ShaderStage::Type stageType)
		{
			auto& stage = mStages[(int)stageType];

			regex re(R"(@In\s*\(\s*([\w\d]+)\s*\))");
			smatch match;

			auto src = stage.source;
			while (regex_search(src, match, re))
			{
				auto attrib = match.str(1);

				// Look for next match
				src = match.suffix().str();
			}
		}

		/*
		 * Parse files and get information, check against spec, and build final sources.
		 *
		 */
		void Parser::build()
		{
			mErrors.clear();
			mWarnings.clear();

			// Check required stages are present
			if (mStages[(int)ShaderStage::Type::Vertex].source == "")
			{
				if (mName == "")
				{
					THROW_MPP_PROGRAM("No vertex shader was given for this program.", __LINE__, __FILE__, __FUNCTION__);
				}
				else
				{
					THROW_MPP_PROGRAM("No vertex shader was given for program '" + mName + "'.", __LINE__, __FILE__, __FUNCTION__);
				}
			}
			if (mStages[(int)ShaderStage::Type::Fragment].source == "")
			{
				if (mName == "")
				{
					THROW_MPP_PROGRAM("No fragment shader was given for this program.", __LINE__, __FILE__, __FUNCTION__);
				}
				else
				{
					THROW_MPP_PROGRAM("No fragment shader was given for program '" + mName + "'.", __LINE__, __FILE__, __FUNCTION__);
				}
			}

			// Set vertex shader attributes
			setInAttributesToMeshSpecification(ShaderStage::Type::Vertex);
			setOutAttributesToUsage(ShaderStage::Type::Vertex);

			// Set geometry shader attributes (if required)
			if (mStages[(int)ShaderStage::Type::Geometry].source != "")
			{
				setInAttributesToPreviousStage(ShaderStage::Type::Geometry);
				setOutAttributesToUsage(ShaderStage::Type::Geometry);
			}

			// Set fragment shader attributes
			setInAttributesToPreviousStage(ShaderStage::Type::Fragment);
			setOutAttributesToUsage(ShaderStage::Type::Fragment);

			// Check unused attributes
			// TODO: ...

			// Add attributes in and do token replacements
			// TODO: ...
		}

		vector<string> const& Parser::getErrors() const
		{
			return mErrors;
		}

		vector<string> const& Parser::getWarnings() const
		{
			return mWarnings;
		}

	}
}