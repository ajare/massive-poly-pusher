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

	ProgrammaticMaterialStream::ProgrammaticMaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec, string const& vertexShader, bool vertexShaderIsFile, string const& fragmentShader, bool fragmentShaderIsFile)
		: MaterialStream(resourceMgr, program2d, meshSpec, vertexShader, vertexShaderIsFile, fragmentShader, fragmentShaderIsFile)
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

	void ProgrammaticMaterialStream::setTextureChild(string const& sampler, string const& resource)
	{
		mTextures[sampler] = make_pair(resource, true);
	}

	void ProgrammaticMaterialStream::setTexture(string const& sampler, string const& texture)
	{
		mTextures[sampler] = make_pair(texture, false);
	}

	void ProgrammaticMaterialStream::useDefaultTexture()
	{
		mTextures["TEX1"] = make_pair("__mpp_tex_none__", false);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, int32 value)
	{
		mUniforms.setUniform(name, value);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, uint32 value)
	{
		mUniforms.setUniform(name, value);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, float value)
	{
		mUniforms.setUniform(name, value);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, glm::vec3 const& value)
	{
		mUniforms.setUniform(name, value);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, glm::vec4 const& value)
	{
		mUniforms.setUniform(name, value);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, size_t count, int32 const* values)
	{
		mUniforms.setUniform(name, count, values);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, size_t count, uint32 const* values)
	{
		mUniforms.setUniform(name, count, values);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, size_t count, float const* values)
	{
		mUniforms.setUniform(name, count, values);
	}
}