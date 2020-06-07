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
		string Parser::stripComments(std::string const& src)
		{
			string stripped = src;

			regex re(R"(\/\*.*\*\/)");

			regex_replace(stripped, re, src);

			return stripped;
		}

		/*
		 * Get the in attributes which are used, and hence require
		 * a declaration.
		 *
		 */
		void Parser::getInAttributes(ShaderStage& stage)
		{
			stage.inAttribs.clear();

			regex re(R"(@In\s*\(\s*([\w\d]+)\s*\))");
			smatch match;

			auto src = stage.source;
			while (regex_search(src, match, re))
			{
				auto attrib = match.str(1);

				// Look for next match
				next_match:	src = match.suffix().str();
			}
		}

		/*
		 * Get the out attributes which are used, and hence require
		 * a declaration.  These require a type.
		 *
		 */
		void Parser::getOutAttributes(ShaderStage& stage)
		{
			stage.outAttribs.clear();

			regex re(R"(@Out\s*\(\s*([\w\d]+\s+)?([\w\d]+)\s*\))");
			smatch match;

			auto src = stage.source;
			while (regex_search(src, match, re))
			{
				if (match.size() == 2)
				{
					// Type not declared.
					auto attrib = match.str(1);
				}
				else
				{
					auto type = match.str(1);
					auto attrib = match.str(2);
				}

				// Look for next match
				next_match:	src = match.suffix().str();
			}
		}

		/*
		 * Get attributes used from the source code.
		 *
		 */
		void Parser::parseAttributeUsage(ShaderStage::Type stageType)
		{
			auto& stage = mStages[(int)stageType];
			
			if (stage.source == "")
			{
				if (stageType == ShaderStage::Type::Vertex)
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
				else if (stageType == ShaderStage::Type::Fragment)
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
			}

			// Remove comments

			getInAttributes(stage);
			getOutAttributes(stage);
		}

		/*
		 * Parse files and get information, check against spec, and build final sources.
		 *
		 */
		void Parser::build()
		{
			mErrors.clear();
			mWarnings.clear();

			// Parse shaders and determine which attributes they use
			parseAttributeUsage(ShaderStage::Type::Vertex);
			parseAttributeUsage(ShaderStage::Type::Fragment);

			// Find unused attributes.  Some attributes may not be used in earlier
			// shaders (vertex/geometry) and simply be passed through to be used by
			// later stages.  But we need to ensure these are not explicitly declared
			// as "non-attribute" outputs in an earlier stage.
			//checkUnusedAttributes(...);
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