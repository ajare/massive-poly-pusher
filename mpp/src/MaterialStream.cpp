#include "mpp/MaterialStream.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	MaterialStream::MaterialStream(ResourceManager* resourceMgr)
		: ResourceStream(resourceMgr, "Material")
	{
	}
	
	/*
	 * Constructor.
	 *
	 */
	MaterialStream::MaterialStream(ResourceManager* resourceMgr, string const& program)
		: MaterialStream(resourceMgr)
	{
		setProgram(program);
	}

	MaterialStream::MaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec, string const& vertexShader, bool vertexShaderIsFile, string const& fragmentShader, bool fragmentShaderIsFile)
		: MaterialStream(resourceMgr)
	{
		setProgram(program2d, meshSpec, vertexShader, vertexShaderIsFile, fragmentShader, fragmentShaderIsFile);
	}

	/*
	 * Constructor.
	 *
	 */
	MaterialStream::MaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec)
		: MaterialStream(resourceMgr)
	{
		setProgram(program2d, meshSpec);
	}

	/*
	 * Constructor.
	 *
	 */
	MaterialStream::MaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec, set<string> const& tags)
		: MaterialStream(resourceMgr)
	{
		setProgram(program2d, meshSpec, tags);
	}

	/*
	 * Get name
	 *
	 */
	string const& MaterialStream::getName() const
	{
		return mName;
	}

	/*
	 * Set program
	 *
	 */
	void MaterialStream::setProgram(string const& program)
	{
		mProgram.resourceExists = true;
		mProgram.existingResource = program;
	}

	/*
	 * Set program
	 *
	 */ 
	void MaterialStream::setProgram(bool is2d, mpp::mesh::MeshSpecification const& spec, set<string> const& tags)
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

	void MaterialStream::setProgram(bool is2d, mesh::MeshSpecification const& spec, std::string const& vertexShader, bool vertexShaderIsFile, std::string const& fragmentShader, bool fragmentShaderIsFile)
	{
		mProgram.resourceExists = false;
		mProgram.is2d = is2d;
		mProgram.spec = spec;
		mProgram.vertexShader = { vertexShaderIsFile, vertexShader };
		mProgram.fragmentShader = { fragmentShaderIsFile, fragmentShader };
	}

	void MaterialStream::setProgram(bool is2d, mesh::MeshSpecification const& spec)
	{
		mProgram.resourceExists = false;
		mProgram.is2d = is2d;
		mProgram.spec = spec;
		mProgram.vertexShader = { false, "" };
		mProgram.fragmentShader = { false, "" };
	}

	/*
	 * Get program.
	 *
	 */
	MaterialStream::ProgramOptions const& MaterialStream::getProgramOptions() const
	{
		return mProgram;
	}

	/*
	 * Get program uniforms.
	 *
	 */
	UniformCollection const& MaterialStream::getUniforms() const
	{
		return mUniforms;
	}

	/*
	 * Get textures.
	 *
	 */
	map<string, pair<string, bool>> const& MaterialStream::getTextures() const
	{
		return mTextures;
	}
}