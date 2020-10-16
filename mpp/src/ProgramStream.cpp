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
	ProgramStream::ProgramStream(ResourceManager* resourceMgr, shared_ptr<program::Parser> parser, set<string> const& attribs)
		: ResourceStream(resourceMgr)
		, mParser(parser)
		, mAttribs(attribs)
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

	/*
	 * Get all source concatenated 
	 *
	 */
	string ProgramStream::getConcatenatedSource()
	{
		load();
		return getVertexSource() + getFragmentSource();
	}

	/*
	 * Load.
	 *
	 */
	void ProgramStream::loadImpl()
	{
		mParser->build(mAttribs);
		setVertexSource(mParser->getGeneratedVertexSource());
		setFragmentSource(mParser->getGeneratedFragmentSource());
	}

	vector<program::Attribute> ProgramStream::getInAttributes() const
	{
		return mParser->getInAttributes();
	}

	vector<string> ProgramStream::getUniforms() const
	{
		return mParser->getUniforms();
	}

	vector<string> ProgramStream::getTextures() const
	{
		return mParser->getTextures();
	}

}