#include "ProgramInformation.h"

using namespace std;

/*
 * Constructor.
 *
 */
ProgramInformation::ProgramInformation(string const& name)
	: mName(name)
	, mVertexShader("")
	, mFragmentShader("")
{
}

/*
 * Get name.
 * 
 */
string const& ProgramInformation::getName() const
{
	return mName;
}

/*
 * Set vertex shader used, if there is a specific one.  This is a filename.
 *
 */
void ProgramInformation::setVertexShader(string const& shader)
{
	mVertexShader = shader;
}

/*
 * Set fragment shader used, if there is a specific one.  This is a filename.
 *
 */
void ProgramInformation::setFragmentShader(string const& shader)
{
	mFragmentShader = shader;
}

void ProgramInformation::setTexture(string const& binding, string const &value)
{
	mTextures[binding] = value;
}

map<string, string> const& ProgramInformation::getTextures() const
{
	return mTextures;
}

void ProgramInformation::setUniform(string const& binding, string const &value)
{
	mUniforms[binding] = value;
}

map<string, string> const& ProgramInformation::getUniforms() const
{
	return mUniforms;
}
