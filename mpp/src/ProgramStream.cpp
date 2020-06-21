#include <iterator>

#include "mpp/ProgramStream.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{

	/*
 	 * Constructor.
	 *
	 */
	ProgramStream::ProgramStream()
	{
	}

	/*
	 * Set vertex shader source.
	 *
	 */
	void ProgramStream::setVertexSource(string const& src)
	{
		mVertexSource = src;
	}

	/*
	* Set fragment shader source.
	*
	*/
	void ProgramStream::setFragmentSource(string const& src)
	{
		mFragmentSource = src;
	}

	/*
	 * Get resource stream type.
	 *
	 */
	string ProgramStream::getType()
	{
		return "Program";
	}

	/*
	 * Get vertex shader source.
	 *
	 */
	string const& ProgramStream::getVertexSource() const
	{
		return mVertexSource;
	}

	/*
	 * Get fragment shader source.
	 *
	 */
	string const& ProgramStream::getFragmentSource() const
	{
		return mFragmentSource;
	}

}