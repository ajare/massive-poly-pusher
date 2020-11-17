#include "mpp/MaterialStream.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	MaterialStream::MaterialStream(ResourceManager* resourceMgr)
		: ResourceStream(resourceMgr, "Material")
	{
	}
	
	/*
	 * Get name
	 *
	 */
	string const& MaterialStream::getName() const
	{
		return mName;
	}

	/*
	 * Get program.
	 *
	 */
	MaterialStream::ProgramOptions const& MaterialStream::getProgramOptions() const
	{
		return mQualitySettings[mQualitySetting].program;
	}

	/*
	 * Get program uniforms.
	 *
	 */
	UniformCollection const& MaterialStream::getUniforms() const
	{
		return mQualitySettings[mQualitySetting].uniforms;
	}

	/*
	 * Get textures.
	 *
	 */
	map<string, pair<string, bool>> const& MaterialStream::getTextures() const
	{
		return mQualitySettings[mQualitySetting].textures;
	}

	uint32_t MaterialStream::createQualitySetting(string const& name)
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