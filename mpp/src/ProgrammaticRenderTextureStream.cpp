#include "mpp/ProgrammaticRenderTextureStream.h"

using namespace std;

namespace mpp
{

	ProgrammaticRenderTextureStream::ProgrammaticRenderTextureStream(ResourceManager* resourceMgr)
		: RenderTextureStream(resourceMgr)
	{
	}

	void ProgrammaticRenderTextureStream::setWidth(size_t width, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.width = width;
	}

	void ProgrammaticRenderTextureStream::setHeight(size_t height, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.height = height;
	}

	void ProgrammaticRenderTextureStream::setDepthBuffer(bool use)
	{
		mUseDepthBuffer = use;
	}

	void ProgrammaticRenderTextureStream::setNumAttachments(size_t numAttachments)
	{
		mNumAttachments = numAttachments;
	}
}