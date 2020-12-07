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
	 * Set geometry shader source.
	 *
	 */
	void ProgramStream::setGeometrySource(string const& src)
	{
		mGeometrySource = src;
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
	 * Get geometry shader source.
	 *
	 */
	string const& ProgramStream::getGeometrySource() const
	{
		return mGeometrySource;
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
	ProgramStream::Shader const& ProgramStream::getVertexShader() const
	{
		return mQualitySettings[mQualitySetting].vertexShader;
	}

	ProgramStream::Shader const& ProgramStream::getGeometryShader() const
	{
		return mQualitySettings[mQualitySetting].geometryShader;
	}

	ProgramStream::Shader const& ProgramStream::getFragmentShader() const
	{
		return mQualitySettings[mQualitySetting].fragmentShader;
	}
	*/
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

		parser->build(mQualitySettings[mQualitySetting].attribs);

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

		if (name != "")
		{
			mQualityNames[name] = qualityId;
		}

		mQualitySettings.push_back(QualitySetting());
		return qualityId;
	}
}