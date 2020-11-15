#include <cassert>

#include "mpp/PostEffectStream.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	PostEffectStream::PostEffectStream(ResourceManager* resourceMgr)
		: ResourceStream(resourceMgr, "PostEffect")
	{
		mQualitySettings.resize(1);
	}

	void PostEffectStream::loadImpl()
	{
		// Test gamma effect
		mInputs.push_back(
		{
			ImageType::Colour
		});

		mOutput.type = ImageType::Colour;
		mOutput.format = "RGB";
	}

	uint32_t PostEffectStream::createQualitySetting(string const& name)
	{
		auto qualityId = mQualitySettings.size();
		mQualityNames[name] = qualityId;

		mQualitySettings.push_back(QualitySetting());
		return qualityId;
	}
}