#include "ShaderStage.h"
#include "MppProgramException.h"

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

	}
}