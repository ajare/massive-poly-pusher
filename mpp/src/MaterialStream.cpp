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