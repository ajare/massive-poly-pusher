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

	SamplerParams const& SamplerStream::getParams(uint32_t quality) const
	{
		return mQualitySettings[quality].params;
	}
}