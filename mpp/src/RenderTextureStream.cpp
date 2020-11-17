#include <cassert>

#include "mpp/RenderTextureStream.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	RenderTextureStream::RenderTextureStream(ResourceManager* resourceMgr)
		: ResourceStream(resourceMgr, "RenderTexture")
	{
	}

	/*
	 * Load data.  Already loaded in constructor!
	 *
	 */
	void RenderTextureStream::loadImpl()
	{
	}

	/*
	 * Get texture width.
	 *
	 */
	size_t RenderTextureStream::getWidth() const
	{
		return mQualitySettings[mQualitySetting].width;
	}

	/*
	 * Get texture height.
	 *
	 */
	size_t RenderTextureStream::getHeight() const
	{
		return mQualitySettings[mQualitySetting].height;
	}

	bool RenderTextureStream::useDepthBuffer() const
	{
		return mUseDepthBuffer;
	}

	size_t RenderTextureStream::getNumAttachments() const
	{
		return mNumAttachments;
	}

	uint32_t RenderTextureStream::createQualitySetting(string const& name)
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