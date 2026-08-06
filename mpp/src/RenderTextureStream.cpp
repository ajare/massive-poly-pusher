#include <cassert>

#include "mpp/RenderTextureStream.h"

using namespace std;

namespace mpp
{

	RenderTextureStream::RenderTextureStream(ResourceManager* resourceMgr)
		: ResourceStream(resourceMgr, "RenderTexture")
		, mDepthAttachment(RenderTextureDepthAttachment::None)
		, mNumAttachments(0)

	{
	}

	void RenderTextureStream::loadImpl()
	{
		// Nothing to do here.
	}

	uint32_t RenderTextureStream::getInternalFormat() const
	{
		return mQualitySettings[mQualitySetting].internalFormat;
	}

	uint32_t RenderTextureStream::getTarget() const
	{
		return mQualitySettings[mQualitySetting].target;
	}

	size_t RenderTextureStream::getWidth() const
	{
		return mQualitySettings[mQualitySetting].width;
	}

	size_t RenderTextureStream::getHeight() const
	{
		return mQualitySettings[mQualitySetting].height;
	}

	size_t RenderTextureStream::getDepth() const
	{
		return mQualitySettings[mQualitySetting].depth;
	}

	size_t RenderTextureStream::getBitsPerPixel() const
	{
		return mQualitySettings[mQualitySetting].bitsPerPixel;
	}

	uint32_t RenderTextureStream::getPixelFormat() const
	{
		return mQualitySettings[mQualitySetting].pixelFormat;
	}

	uint32_t RenderTextureStream::getPixelDataType() const
	{
		return mQualitySettings[mQualitySetting].pixelDataType;
	}

	TextureParams const& RenderTextureStream::getParams() const
	{
		return mQualitySettings[mQualitySetting].params;
	}

	string const& RenderTextureStream::getSampler() const
	{
		return mQualitySettings[mQualitySetting].sampler;
	}

	bool RenderTextureStream::useDepthBuffer() const
	{
		return mDepthAttachment != RenderTextureDepthAttachment::None;
	}

	RenderTextureDepthAttachment RenderTextureStream::getDepthAttachment() const
	{
		return mDepthAttachment;
	}

	RenderTextureDepthParams const& RenderTextureStream::getDepthParams() const
	{
		return mDepthParams;
	}

	size_t RenderTextureStream::getNumAttachments() const
	{
		return mNumAttachments;
	}

	uint32_t RenderTextureStream::getSamples() const
	{
		return mSamples;
	}

	uint32_t RenderTextureStream::createQualitySetting(string const& name)
	{
		auto qualityId = (uint32_t)mQualitySettings.size();
		mQualityNames[name] = qualityId;

		mQualitySettings.push_back(QualitySetting());
		return qualityId;
	}
}