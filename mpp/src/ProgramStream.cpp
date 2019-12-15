#include <iterator>

#include "mpp/ProgramStream.h"

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
	 * Load program from strings.
	 *
	 */
	void ProgramStream::loadFromStrings(string const& vertSrc, string const& fragSrc)
	{
		mVertexSource = vertSrc;
		mFragmentSource = fragSrc;
	}

	/*
	 * Load program from files.
	 *
	 */
	void ProgramStream::loadFromFiles(string const& vertFile, string const& fragFile)
	{
		ifstream vf, ff;

		// Vertex shader
		vf.open(vertFile);
		if (!vf.is_open())
		{
			throw exception("ProgramStream::loadFromFiles() vertFile is not open.");
		}

		mVertexSource = string((istreambuf_iterator<char>(vf)), istreambuf_iterator<char>());
		vf.close();

		// Fragment shader
		ff.open(fragFile);
		if (!ff.is_open())
		{
			throw exception("ProgramStream::loadFromFiles() fragFile is not open.");
		}

		mFragmentSource = string((istreambuf_iterator<char>(ff)), istreambuf_iterator<char>());
		ff.close();
	}

	/*
	 * Add information about one of the program attributes.
	 *
	 */
	void ProgramStream::addAttribInfo(string const& name, int size, int offset, bool normalise)
	{
		AttribInfo ainfo;
		ainfo.name = name;
		ainfo.size = size;
		ainfo.offset = offset;
		ainfo.normalise = normalise;

		mAttribInfo.push_back(ainfo);
	}

	/*
	 * Get attribute info.
	 *
	 */
	vector<ProgramStream::AttribInfo> const& ProgramStream::getAttribInfo() const
	{
		return mAttribInfo;
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