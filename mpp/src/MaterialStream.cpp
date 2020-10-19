#include "mpp/MaterialStream.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	MaterialStream::MaterialStream(ResourceManager* resourceMgr)
		: ResourceStream(resourceMgr)
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

	MaterialStream::MaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec, string const& vertexShader, string const& fragmentShader, bool shadersAreFiles)
		: MaterialStream(resourceMgr)
	{
		setProgram(program2d, meshSpec, vertexShader, fragmentShader, shadersAreFiles);
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
	 * Get resource type.
	 *
	 */
	std::string MaterialStream::getType()
	{
		return "Material";
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

	void MaterialStream::setProgram(bool is2d, mesh::MeshSpecification const& spec, std::string const& vertexShader, std::string const& fragmentShader, bool shadersAreFiles)
	{
		mProgram.resourceExists = false;
		mProgram.shadersAreFiles = shadersAreFiles;
		mProgram.is2d = is2d;
		mProgram.spec = spec;
		mProgram.vertexShader = vertexShader;
		mProgram.fragmentShader = fragmentShader;
	}

	void MaterialStream::setProgram(bool is2d, mesh::MeshSpecification const& spec)
	{
		mProgram.resourceExists = false;
		mProgram.is2d = is2d;
		mProgram.spec = spec;
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
	map<string, MaterialStream::Uniform<float>> const& MaterialStream::getFloatUniforms() const
	{
		return mFloatUniforms;
	}

	/*
	 * Get textures.
	 *
	 */
	map<string, string> const& MaterialStream::getTextures() const
	{
		return mTextures;
	}
}