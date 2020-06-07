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
			stage.source = src;
			stage.in.clear();
			stage.out.clear();
		}

		/*
		 * Set geometry shader source.
		 *
		 */
		void Parser::setGeometrySource(string const& src)
		{
			auto& stage = mStages[(int)ShaderStage::Type::Geometry];

			stage.type = ShaderStage::Type::Geometry;
			stage.source = src;
			stage.in.clear();
			stage.out.clear();
		}

		/*
		 * Set fragment shader source.
		 *
		 */		
		void Parser::setFragmentSource(string const& src)
		{
			auto& stage = mStages[(int)ShaderStage::Type::Fragment];

			stage.type = ShaderStage::Type::Fragment;
			stage.source = src;
			stage.in.clear();
			stage.out.clear();
		}

		/*
		 * Set mesh specification to build program with.
		 *
		 */
		void Parser::setMeshSpecification(mesh::MeshSpecification const& spec)
		{
			mSpecification = spec;

			// Read in attributes
			mSpecificationAttributes.clear();
			for (int i = 0; i < mSpecification.getNumVertexBufferAttributeLayouts(); ++i)
			{
				auto const& layout = mSpecification.getVertexBufferAttributeLayout(i);

				for (int j = 0; j < layout.getNumAttributes(); ++j)
				{
					auto const& attrib = layout.getAttribute(j);

					switch (attrib.component)
					{
					case mpp::mesh::Vertex::Component::Position2:
					case mpp::mesh::Vertex::Component::Position3:
					case mpp::mesh::Vertex::Component::Position4:
						mSpecificationAttributes.insert(AttributeType::Position);
						break;

					case mpp::mesh::Vertex::Component::Normal3:
					case mpp::mesh::Vertex::Component::Normal4:
						mSpecificationAttributes.insert(AttributeType::Normal);
						break;

					case mpp::mesh::Vertex::Component::TexCoord2:
					case mpp::mesh::Vertex::Component::TexCoord3:
					case mpp::mesh::Vertex::Component::TexCoord4:
						mSpecificationAttributes.insert(AttributeType::TexCoords);
						break;

					case mpp::mesh::Vertex::Component::Colour1:
					case mpp::mesh::Vertex::Component::Colour3:
					case mpp::mesh::Vertex::Component::Colour4:
						mSpecificationAttributes.insert(AttributeType::Colour);
						break;
					}
				}
			}
		}

		/*
		 * Check which attributes have been declared in the mesh specification, but
		 * are not actually used in the shader.  This is not an error, but will
		 * generate a warning.
		 *
		 */
		void Parser::checkUnusedAttributes(set<AttributeType> const& attribs)
		{
			for (int i = 0; i < mSpecification.getNumVertexBufferAttributeLayouts(); ++i)
			{
				auto const& layout = mSpecification.getVertexBufferAttributeLayout(i);

				for (int j = 0; j < layout.getNumAttributes(); ++j)
				{
					auto const& attrib = layout.getAttribute(j);

					switch (attrib.component)
					{
					case mpp::mesh::Vertex::Component::Position2:
					case mpp::mesh::Vertex::Component::Position3:
					case mpp::mesh::Vertex::Component::Position4:
						if (find(attribs.begin(), attribs.end(), AttributeType::Position) == attribs.end())
						{
							mWarnings.push_back(mpp::mesh::Vertex::getComponentName(attrib.component) + " is not used.");
						}
						break;

					case mpp::mesh::Vertex::Component::Normal3:
					case mpp::mesh::Vertex::Component::Normal4:
						if (find(attribs.begin(), attribs.end(), AttributeType::Normal) == attribs.end())
						{
							mWarnings.push_back(mpp::mesh::Vertex::getComponentName(attrib.component) + " is not used.");
						}
						break;

					case mpp::mesh::Vertex::Component::TexCoord2:
					case mpp::mesh::Vertex::Component::TexCoord3:
					case mpp::mesh::Vertex::Component::TexCoord4:
						if (find(attribs.begin(), attribs.end(), AttributeType::TexCoords) == attribs.end())
						{
							mWarnings.push_back(mpp::mesh::Vertex::getComponentName(attrib.component) + " is not used.");
						}
						break;

					case mpp::mesh::Vertex::Component::Colour1:
					case mpp::mesh::Vertex::Component::Colour3:
					case mpp::mesh::Vertex::Component::Colour4:
						if (find(attribs.begin(), attribs.end(), AttributeType::Colour) == attribs.end())
						{
							mWarnings.push_back(mpp::mesh::Vertex::getComponentName(attrib.component) + " is not used.");
						}
						break;
					}
				}
			}
		}

		/*
		 * Get the attributes which are spcecifically declared
		 *
		 */
		void Parser::getDeclaredAttributes(ShaderStage& stage)
		{
			regex re(R"(@@(In|Out)\s*\(\s*([\w\d]+)\s*\)\s*=\s*([\w\d]+))");
			smatch match;

			auto src = stage.source;
			while (regex_search(src, match, re))
			{
				auto qualifier = match.str(1);
				auto attrib = match.str(2);
				auto type = match.str(3);

				// If this is an in attribute, then it must be in the spec if
				// a vertex shader, or in the out attributes of the previous stage.
				if (qualifier == "In")
				{
					mErrors.push_back("@In attributes should not be declared.");
				}
				else if (qualifier == "Out")
				{
					// TODO: add to out list
				}
				else
				{
					THROW_MPP_PROGRAM("Unknown qualifier: " + qualifier, __LINE__, __FILE__, __FUNCTION__);
				}

				// Look for next match
				src = match.suffix().str();
			}
		}

		/*
		 * Get the attributes which are used, and hence require
		 * a declaration.
		 *
		 */
		void Parser::getUsedAttributes(ShaderStage& stage)
		{
			regex re(R"([^@]@(In|Out)\s*\(\s*([\w\d]+)\s*\))");
			smatch match;

			auto src = stage.source;
			while (regex_search(src, match, re))
			{
				auto qualifier = match.str(1);
				auto attrib = match.str(2);

				// Check attrib is one of the allowed ones, and if so,
				// get its corresponding AttributeType
				auto attribIt = stage.definedAttributes.find(attrib);
				if (attribIt == stage.definedAttributes.end())
				{
					mErrors.push_back("Unknown attribute '" + attrib + "' used.");
					goto next_match;
				}

				// Check attribs is in the mesh specification
				if (stage.type == ShaderStage::Type::Vertex && qualifier == "In")
				{
					auto attribIdIt = mSpecificationAttributes.find(attribIt->second);
					if (attribIdIt == mSpecificationAttributes.end())
					{
						mErrors.push_back("Attribute '" + attrib + "' is not part of the mesh specification.");
						goto next_match;
					}
				}

				if (qualifier == "In")
				{
					stage.in.insert(attribIt->second);
				}
				else if (qualifier == "Out")
				{
					stage.out.insert(attribIt->second);
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
			
			stage.definedAttributes = mStandardAttributes;
			stage.in.clear();
			stage.out.clear();
			
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

			getDeclaredAttributes(stage);
			getUsedAttributes(stage);
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