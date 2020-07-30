#include <iterator>

#include "mpp/ProgramProgramStream.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	ProgramProgramStream::ProgramProgramStream(std::shared_ptr<program::Parser> parser, set<string> const& attribs)
		: mParser(parser)
		, mAttribs(attribs)
	{
	}

	/*
	 * Get resource stream type.
	 *
	 */
	string ProgramProgramStream::getType()
	{
		return "ProgramProgram";
	}

	/*
	 * Load.
	 *
	 */
	void ProgramProgramStream::loadImpl()
	{
		mParser->build(mAttribs);
		setVertexSource(mParser->getGeneratedVertexSource());
		setFragmentSource(mParser->getGeneratedFragmentSource());
	}

	vector<program::Attribute> ProgramProgramStream::getInAttributes() const
	{
		return mParser->getInAttributes();
	}

	vector<string> ProgramProgramStream::getUniforms() const
	{
		return mParser->getUniforms();
	}

	vector<string> ProgramProgramStream::getTextures() const
	{
		return mParser->getTextures();
	}

}