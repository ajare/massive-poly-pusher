#include "MaterialInformation.h"

using namespace std;

/*
 * Constructor.
 *
 */
MaterialInformation::MaterialInformation(string const& name)
	: mName(name)
	, mProgram("")
{
}

/*
 * Get name.
 *
 */
string const& MaterialInformation::getName() const
{
	return mName;
}

/*
 * Set program.
 *
 */
void MaterialInformation::setProgram(string const& program)
{
	mProgram = program;
}

/*
 * Get name.
 *
 */
string const& MaterialInformation::getProgram() const
{
	return mProgram;
}
