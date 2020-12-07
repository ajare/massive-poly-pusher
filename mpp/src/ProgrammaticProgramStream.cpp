#include "mpp/ProgrammaticProgramStream.h"

using namespace std;

namespace mpp
{

	ProgrammaticProgramStream::ProgrammaticProgramStream(ResourceManager* resourceMgr)
		: ProgramStream(resourceMgr)
	{
		createQualitySetting("");
	}

	void ProgrammaticProgramStream::setParser(shared_ptr<program::Parser> parser, uint32_t quality)
	{
		mQualitySettings[quality].parser = parser;
	}

	void ProgrammaticProgramStream::setAttribs(set<string> const& attribs, uint32_t quality)
	{
		mQualitySettings[quality].attribs = attribs;
	}
}