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
	ProgramStream::ProgramStream(ResourceManager* resourceMgr)
		: ResourceStream(resourceMgr, "Program")
	{
		mQualitySettings.resize(1);
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
		return getVertexSource() + getFragmentSource();
	}

	/*
	 * Load.
	 *
	 */
	void ProgramStream::loadImpl()
	{
		auto parser = mQualitySettings[mQualitySetting].parser;

		parser->build(mAttribs);

		setVertexSource(parser->getGeneratedVertexSource());
		setFragmentSource(parser->getGeneratedFragmentSource());
	}

	vector<program::Attribute> ProgramStream::getInAttributes() const
	{
		auto parser = mQualitySettings[mQualitySetting].parser;
		return parser->getInAttributes();
	}

	vector<string> ProgramStream::getUniforms() const
	{
		auto parser = mQualitySettings[mQualitySetting].parser;
		return parser->getUniforms();
	}

	vector<string> ProgramStream::getTextures() const
	{
		auto parser = mQualitySettings[mQualitySetting].parser;
		return parser->getTextures();
	}

	uint32_t ProgramStream::createQualitySetting(string const& name)
	{
		auto qualityId = mQualitySettings.size();
		mQualityNames[name] = qualityId;

		mQualitySettings.push_back(QualitySetting());
		return qualityId;
	}
}