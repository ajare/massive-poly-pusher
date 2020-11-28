#include <cassert>

#include "utils/FileSystem.h"

#include "mpp/ProgrammaticStringStream.h"
#include "mpp/ResourceManager.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{

	ProgrammaticStringStream::ProgrammaticStringStream(ResourceManager* resourceMgr)
		: StringStream(resourceMgr)
	{
		createQualitySetting("");
	}

	void ProgrammaticStringStream::loadImpl()
	{
		auto& qs = mQualitySettings[mQualitySetting];
		
		if (qs.isFile)
		{
			// Load file
			qs.data = utils::FileSystem::readTextFile(qs.file);
		}
	}

	void ProgrammaticStringStream::setString(string const& data, uint32_t quality)
	{
		mQualitySettings[quality].data = data;
		mQualitySettings[quality].isFile = false;
	}

	void ProgrammaticStringStream::setFile(string const& file, uint32_t quality)
	{
		mQualitySettings[quality].file = file;
		mQualitySettings[quality].isFile = true;
	}
}