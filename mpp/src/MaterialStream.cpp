#include "mpp/MaterialStream.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	MaterialStream::MaterialStream()
	{
	}
	
	/*
	 * Constructor.
	 *
	 */
	MaterialStream::MaterialStream(string const& program)
		: MaterialStream()
	{
		setProgram(program);
	}

	/*
	 * Constructor.
	 *
	 */
	MaterialStream::MaterialStream(bool program2d, mesh::MeshSpecification const& meshSpec)
		: MaterialStream()
	{
		setProgram(program2d, meshSpec, {});
	}

	/*
	 * Constructor.
	 *
	 */
	MaterialStream::MaterialStream(bool program2d, mesh::MeshSpecification const& meshSpec, set<string> const& tags)
		: MaterialStream()
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
		mProgram = program;
	}

	/*
	 * Set program
	 *
	 */
	void MaterialStream::setProgram(bool is2d, mpp::mesh::MeshSpecification const& spec, set<string> const& tags)
	{
		string prefix = "__mpp_";
		mProgram = spec.getDescriptor(prefix + (is2d ? "p2d_" : "p3d_"));

		for (auto const& tag : tags)
		{
			if (tag == "diffuse")
			{
				mProgram += "_d";
			}
		}

		mProgram += "__";
	}

	/*
	 * Get program.
	 *
	 */
	string const& MaterialStream::getProgram() const
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