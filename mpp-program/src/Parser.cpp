#include <set>
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
		 * Set vertex shader source.
		 *
		 */
		void Parser::setVertexSource(string const& src)
		{
			mSources[(int)ShaderStage::Vertex] = src;
		}

		/*
		 * Set geometry shader source.
		 *
		 */
		void Parser::setGeometrySource(string const& src)
		{
			mSources[(int)ShaderStage::Geometry] = src;
		}

		/*
		 * Set fragment shader source.
		 *
		 */		
		void Parser::setFragmentSource(string const& src)
		{
			mSources[(int)ShaderStage::Fragment] = src;
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
		 * Get attributes used from the source code.
		 *
		 */
		void Parser::parseAttributeUsage(ShaderStage stage)
		{
			// Get source
			auto src = mSources[(int)stage];
			if (src == "")
			{
				if (stage == ShaderStage::Vertex)
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
				else if (stage == ShaderStage::Fragment)
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
			set<string> attribs{ "POS", "NORMAL", "TEXCOORDS", "COLOUR" };

			regex re(R"(@(In|Out)\s*\(\s*([\w\d]+)\s*\))");
			smatch matches;
			vector<string> usedAttribs, undeclaredAttribs;

			// If this is the vertex shader, check that all the in
			// variables used in source exist in the mesh spec, and store
			// all the out variables for use in the next stage.

			// If it's the next stage (geometry or fragment) compare in
			// variables against the stored variables from the last stage.

			if (regex_search(src, matches, re))
			{
				for (size_t i = 1; i < matches.size(); i += 2)
				{
					auto qualifier = matches[i + 0];
					auto attrib = matches[i + 1];

					if (qualifier == "In")
					{
						// Check mesh specification
						// ...
					}
				}
			}
		}

		/*
		 * Parse files and get information, check against spec, and build final sources.
		 *
		 */
		void Parser::build()
		{
			// Parse shaders and determine which attributes they use
			parseAttributeUsage(ShaderStage::Vertex);
			parseAttributeUsage(ShaderStage::Fragment);

			// Check these against mesh spec to ensure it's sufficient, and optionally warn
			// on any which are not needed.
			// ...
		}
	}
}