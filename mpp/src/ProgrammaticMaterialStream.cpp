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
		: MaterialStream(resourceMgr)
	{
		setProgram(program);
	}

	ProgrammaticMaterialStream::ProgrammaticMaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec, string const& vertexShader, bool vertexShaderIsFile, string const& fragmentShader, bool fragmentShaderIsFile)
		: MaterialStream(resourceMgr)
	{
		setProgram(program2d, meshSpec, vertexShader, vertexShaderIsFile, fragmentShader, fragmentShaderIsFile);
	}

	/*
	 * Constructor.
	 *
	 */
	ProgrammaticMaterialStream::ProgrammaticMaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec)
		: MaterialStream(resourceMgr)
	{
		setProgram(program2d, meshSpec);
	}

	/*
	 * Constructor.
	 *
	 */
	ProgrammaticMaterialStream::ProgrammaticMaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec, set<string> const& tags)
		: MaterialStream(resourceMgr)
	{
		setProgram(program2d, meshSpec, tags);
	}

	/*
	 * Set program
	 *
	 */
	void ProgrammaticMaterialStream::setProgram(string const& program)
	{
		mProgram.resourceExists = true;
		mProgram.existingResource = program;
	}

	/*
	 * Set program
	 *
	 */
	void ProgrammaticMaterialStream::setProgram(bool is2d, mpp::mesh::MeshSpecification const& spec, set<string> const& tags)
	{
		mProgram.resourceExists = true;
		mProgram.is2d = is2d;

		string prefix = "__mpp_";
		mProgram.existingResource = spec.getDescriptor(prefix + (is2d ? "p2d_" : "p3d_"));

		for (auto const& tag : tags)
		{
			if (tag == "diffuse")
			{
				mProgram.existingResource += "_d";
			}
		}

		mProgram.existingResource += "__";
	}

	void ProgrammaticMaterialStream::setProgram(bool is2d, mesh::MeshSpecification const& spec, std::string const& vertexShader, bool vertexShaderIsFile, std::string const& fragmentShader, bool fragmentShaderIsFile)
	{
		mProgram.resourceExists = false;
		mProgram.is2d = is2d;
		mProgram.spec = spec;
		mProgram.vertexShader = { vertexShaderIsFile, vertexShader };
		mProgram.fragmentShader = { fragmentShaderIsFile, fragmentShader };
	}

	void ProgrammaticMaterialStream::setProgram(bool is2d, mesh::MeshSpecification const& spec)
	{
		mProgram.resourceExists = false;
		mProgram.is2d = is2d;
		mProgram.spec = spec;
		mProgram.vertexShader = { false, "" };
		mProgram.fragmentShader = { false, "" };
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

	void ProgrammaticMaterialStream::setUniform(string const& name, int32_t value)
	{
		mUniforms.setUniform(name, value);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, uint32_t value)
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

	void ProgrammaticMaterialStream::setUniform(string const& name, size_t count, int32_t const* values)
	{
		mUniforms.setUniform(name, count, values);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, size_t count, uint32_t const* values)
	{
		mUniforms.setUniform(name, count, values);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, size_t count, float const* values)
	{
		mUniforms.setUniform(name, count, values);
	}
}