#include <fstream>
#include <sstream>

#include "mpp/FileStringStream.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	FileStringStream::FileStringStream(ResourceManager* resourceMgr, string const& filename)
		: StringStream(resourceMgr)
		, mFilename(filename)
	{
	}

	void FileStringStream::loadImpl()
	{
		ifstream ifs(mFilename);

		if (!ifs.is_open())
		{
			THROW_MPP("Could not load " + mFilename, __LINE__, __FILE__, __func__);
		}

		std::stringstream buffer;
		
		buffer << ifs.rdbuf();
		mData = buffer.str();
	}

	void FileStringStream::unloadImpl()
	{
		mData.clear();
	}

	/*
	 * Get resource type.
	 *
	 */
	std::string FileStringStream::getData() const
	{
		return mData;
	}

}