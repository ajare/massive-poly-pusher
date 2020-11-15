#include <cassert>

#include "mpp/RenderTextureStream.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	RenderTextureStream::RenderTextureStream(ResourceManager* resourceMgr, int width, int height, bool useDepthBuffer, size_t numAttachments)
		: ResourceStream(resourceMgr, "RenderTexture")
		, mWidth(width)
		, mHeight(height)
		, mUseDepthBuffer(useDepthBuffer)
		, mNumAttachments(numAttachments)
	{
		mQualitySettings.resize(1);
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
	int RenderTextureStream::getWidth() const
	{
		return mWidth;
	}

	/*
	 * Get texture height.
	 *
	 */
	int RenderTextureStream::getHeight() const
	{
		return mHeight;
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
		mQualityNames[name] = qualityId;

		mQualitySettings.push_back(QualitySetting());
		return qualityId;
	}
}