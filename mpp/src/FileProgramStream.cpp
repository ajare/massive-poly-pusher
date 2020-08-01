#include "mpp/FileProgramStream.h"
#include "mpp/MppException.h"

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
			THROW_MPP_IO("Could not open " + mVertFile, __LINE__, __FILE__, __func__);
		}

		setVertexSource(string((istreambuf_iterator<char>(vf)), istreambuf_iterator<char>()));
		vf.close();

		// Fragment shader
		ff.open(mFragFile);
		if (!ff.is_open())
		{
			THROW_MPP_IO("Could not open " + mFragFile, __LINE__, __FILE__, __func__);
		}

		setFragmentSource(string((istreambuf_iterator<char>(ff)), istreambuf_iterator<char>()));
		ff.close();
	}
}
