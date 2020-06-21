#include "ShaderStage.h"
#include "MppProgramException.h"

#define MPP_PROGRAM_IN_PREFIX				"_mpp_i_"
#define MPP_PROGRAM_OUT_PREFIX				"_mpp_o_"

#define MPP_PROGRAM_UNIFORM_PREFIX			"_mpp_u_"
#define MPP_PROGRAM_TEXTURE_PREFIX			"_mpp_t_"

using namespace std;

namespace mpp
{
	namespace program
	{
		using namespace std;

		bool ShaderStage::required() const 
		{ 
			return type == Type::Vertex || type == Type::Fragment; 
		}

		bool ShaderStage::provided() const 
		{ 
			return source != ""; 
		}

		bool ShaderStage::inAttributeExists(string const& attrib) const
		{
			return find_if(inAttribs.begin(), inAttribs.end(), [attrib](auto const& attrStruct)
			{
				return attrStruct.name == attrib;
			}) != inAttribs.end();
		}

		bool ShaderStage::outAttributeExists(string const& attrib) const
		{
			return find_if(outAttribs.begin(), outAttribs.end(), [attrib](auto const& attrStruct)
			{
				return attrStruct.name == attrib;
			}) != outAttribs.end();
		}

		size_t ShaderStage::getVariableSize(string const& attrib) const
		{
			// In vars
			auto inIt = find_if(inAttribs.begin(), inAttribs.end(), [attrib](auto const& attrStruct)
			{
				return (MPP_PROGRAM_IN_PREFIX + attrStruct.name) == attrib;
			});

			if (inIt != inAttribs.end())
			{
				return mesh::Vertex::getComponentSize(inIt->component);
			}

			// Out vars
			auto outIt = find_if(outAttribs.begin(), outAttribs.end(), [attrib](auto const& attrStruct)
			{
				return (MPP_PROGRAM_OUT_PREFIX + attrStruct.name) == attrib;
			});

			if (outIt != outAttribs.end())
			{
				return mesh::Vertex::getComponentSize(outIt->component);
			}

			// Uniform vars
			auto unIt = find_if(uniforms.begin(), uniforms.end(), [attrib](auto const& uniform)
			{
				return (MPP_PROGRAM_UNIFORM_PREFIX + uniform.name) == attrib;
			});

			if (unIt != uniforms.end())
			{
				// Parse uniform.type.  If it ends in a number, it's that number, else 1
			}

			return 0;
		}

	}
}