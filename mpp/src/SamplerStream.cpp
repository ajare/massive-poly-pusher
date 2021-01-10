#include <cassert>

#include "mpp/SamplerStream.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	SamplerStream::SamplerStream(ResourceManager* resourceMgr)
		: ResourceStream(resourceMgr, "Sampler")
	{
	}

	/*
	* Destructor.
	*
	*/
	SamplerStream::~SamplerStream()
	{
	}

	/*
	 * Load data.
	 *
	 */
	void SamplerStream::loadImpl()
	{
	}

	SamplerParams const& SamplerStream::getParams() const
	{
		return mQualitySettings[mQualitySetting].params;
	}

	uint32_t SamplerStream::createQualitySetting(string const& name)
	{
		auto qualityId = mQualitySettings.size();
		mQualityNames[name] = qualityId;

		mQualitySettings.push_back(QualitySetting());
		return qualityId;
	}
}