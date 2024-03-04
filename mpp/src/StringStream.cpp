#include "mpp/StringStream.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	StringStream::StringStream(ResourceManager* resourceMgr)
		: ResourceStream(resourceMgr, "String")
	{
	}

	uint32_t StringStream::createQualitySetting(string const& name)
	{
		auto qualityId = (uint32_t)mQualitySettings.size();
		mQualityNames[name] = qualityId;

		mQualitySettings.push_back(QualitySetting());
		return qualityId;
	}

	string const& StringStream::getString() const
	{
		return mQualitySettings[mQualitySetting].data;
	}
}