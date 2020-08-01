#include <fstream>
#include <sstream>

#include "mpp/FileDataStream.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	FileDataStream::FileDataStream(string const& filename)
	{
		std::ifstream fin(filename, std::ios::in | std::ios::binary);

		if (fin)
		{
			std::ostringstream contents;

			contents << fin.rdbuf();
			fin.close();

			mFileData = contents.str();
		}
		else
		{
			THROW_MPP_IO("Could not open " + filename, __LINE__, __FILE__, __func__);
		}
	}

	/*
	 * Get data size.
	 *
	 */
	int FileDataStream::getDataSize() const
	{
		return mFileData.length();
	}

	/*
	 * Get data.
	 *
	 */
	int8 const* FileDataStream::getData() const
	{
		return (int8 const*)mFileData.c_str();
	}

}
