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