#include "mpp/Program.h"
#include "mpp/ProgrammaticMaterialStream.h"

using namespace std;

namespace mpp
{

	ProgrammaticMaterialStream::ProgrammaticMaterialStream(ResourceManager* resourceMgr)
		: MaterialStream(resourceMgr)
	{
	}

	/*
	 * Constructor.
	 *
	 */
	ProgrammaticMaterialStream::ProgrammaticMaterialStream(ResourceManager* resourceMgr, string const& program)
		: MaterialStream(resourceMgr, program)
	{
	}

	ProgrammaticMaterialStream::ProgrammaticMaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec, string const& vertexShader, string const& fragmentShader, bool shadersAreFiles)
		: MaterialStream(resourceMgr, program2d, meshSpec, vertexShader, fragmentShader, shadersAreFiles)
	{
	}

	/*
	 * Constructor.
	 *
	 */
	ProgrammaticMaterialStream::ProgrammaticMaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec)
		: MaterialStream(resourceMgr, program2d, meshSpec)
	{
	}

	/*
	 * Constructor.
	 *
	 */
	ProgrammaticMaterialStream::ProgrammaticMaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec, set<string> const& tags)
		: MaterialStream(resourceMgr, program2d, meshSpec, tags)
	{
	}
	void ProgrammaticMaterialStream::setTexture(string const& sampler, string const& texture)
	{
		mTextures[sampler] = texture;
	}

	void ProgrammaticMaterialStream::useDefaultTexture()
	{
		mTextures["TEX1"] = "__mpp_tex_none__";
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