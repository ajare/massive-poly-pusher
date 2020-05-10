#include "mpp/Program.h"
#include "mpp/ProgrammaticMaterialStream.h"

using namespace std;

namespace mpp
{

	void ProgrammaticMaterialStream::setTexture(string const& sampler, string const& texture)
	{
		mTextures[sampler] = texture;
	}

	void ProgrammaticMaterialStream::useDefaultTexture()
	{
		mTextures["tex"] = "__mpp_tex_none";
	}

	void ProgrammaticMaterialStream::setFloatUniform(string const& name, float value)
	{
		Uniform<float> u;
		u.valueCount = 1;
		u.values[0] = value;

		// Mark up name
		string markedUpName = MPP_PROGRAM_MARKUP_UNIFORM(name);
		mFloatUniforms[markedUpName] = u;
	}

	void ProgrammaticMaterialStream::setFloatUniform(string const& name, glm::vec2 const& value)
	{
		Uniform<float> u;

		u.valueCount = 2;
		u.values[0] = value[0];
		u.values[1] = value[1];

		// Mark up name
		string markedUpName = MPP_PROGRAM_MARKUP_UNIFORM(name);
		mFloatUniforms[markedUpName] = u;
	}

	void ProgrammaticMaterialStream::setFloatUniform(string const& name, glm::vec3 const& value)
	{
		Uniform<float> u;

		u.valueCount = 3;
		u.values[0] = value[0];
		u.values[1] = value[1];
		u.values[2] = value[2];

		mFloatUniforms[name] = u;
	}

	void ProgrammaticMaterialStream::setFloatUniform(string const& name, glm::vec4 const& value)
	{
		Uniform<float> u;

		u.valueCount = 4;
		u.values[0] = value[0];
		u.values[1] = value[1];
		u.values[2] = value[2];
		u.values[3] = value[3];

		mFloatUniforms[name] = u;
	}

}