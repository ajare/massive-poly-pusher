#include "mpp/StringProgramStream.h"

using namespace std;

namespace mpp
{
	/*
	* Constructor.
	*
	*/
	StringProgramStream::StringProgramStream(string const& vertString, string const& fragString)
	{
		setVertexSource(vertString);
		setFragmentSource(fragString);
	}

	/*
	* Load strings.  Already done by constructor.
	*
	*/
	void StringProgramStream::loadImpl()
	{
	}
}
