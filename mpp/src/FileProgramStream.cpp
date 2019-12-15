#include "mpp/FileProgramStream.h"

using namespace std;

namespace mpp
{
	/*
	 * Constructor.
	 *
	 */
	FileProgramStream::FileProgramStream(string const& vertFile, string const& fragFile)
		: mVertFile(vertFile)
		, mFragFile(fragFile)
	{
	}

	/*
	 * Load files.
	 *
	 */
	void FileProgramStream::loadImpl()
	{
		ifstream vf, ff;

		// Vertex shader
		vf.open(mVertFile);
		if (!vf.is_open())
		{
			throw exception("ProgramStream::ProgramStream() vertFile is not open.");
		}

		setVertexSource(string((istreambuf_iterator<char>(vf)), istreambuf_iterator<char>()));
		vf.close();

		// Fragment shader
		ff.open(mFragFile);
		if (!ff.is_open())
		{
			throw exception("ProgramStream::ProgramStream() fragFile is not open.");
		}

		setFragmentSource(string((istreambuf_iterator<char>(ff)), istreambuf_iterator<char>()));
		ff.close();
	}
}
