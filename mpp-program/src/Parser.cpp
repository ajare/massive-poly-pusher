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
			: mName("")
		{
		}

		/*
		 * Constructor.
		 *
		 */
		Parser::Parser(std::string const& name)
			: mName(name)
		{
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
		 * Get attributes used from the source code.
		 *
		 */
		void Parser::parseAttributeUsage(ShaderStage::Type stageType)
		{
			// Get source
			auto& stage = mStages[(int)stageType];
			
			auto src = stage.source;

			if (src == "")
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

			// Get used attribs
			map<string, AttributeType> definedAttribs =
			{ 
				{"POS", AttributeType::Position},
				{"NORMAL", AttributeType::Normal},
				{"TEXCOORDS", AttributeType::TexCoords},
				{"COLOUR", AttributeType::Colour}
			};

			regex re(R"(@(In|Out)\s*\(\s*([\w\d]+)\s*\))");
			smatch match;

			while (regex_search(src, match, re))
			{
				auto qualifier = match.str(1);
				auto attrib = match.str(2);

				// Check attrib is one of the predefined ones, and if so,
				// get its corresponding AttributeType
				auto attribIt = definedAttribs.find(attrib);
				if (attribIt == definedAttribs.end())
				{
					mErrors.push_back("Unknown attribute '" + attrib + "' used.");
				}

				// Check attribs is in the mesh specification
				auto attribIdIt = mSpecificationAttributes.find(attribIt->second);
				if (attribIdIt == mSpecificationAttributes.end())
				{
					mErrors.push_back("Attribute '" + attrib + "' is not part of the mesh specification.");
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
				src = match.suffix().str();
			}

			// Find unused attributes
			checkUnusedAttributes(stage.in);
		}

		/*
		 * Parse files and get information, check against spec, and build final sources.
		 *
		 */
		void Parser::build()
		{
			mWarnings.clear();

			// Parse shaders and determine which attributes they use
			parseAttributeUsage(ShaderStage::Type::Vertex);
			parseAttributeUsage(ShaderStage::Type::Fragment);
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