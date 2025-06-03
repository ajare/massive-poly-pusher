#include "mpp/BufferRenderer.h"

namespace mpp
{
	BufferRenderer::BufferRenderer(BufferDataProviderPtr dataProvider)
		: mDataProvider(dataProvider)
	{
	}

	void BufferRenderer::render(RenderSystem* renderSystem)
	{
		renderSystem->renderBufferImmediate(
			mDataProvider->getVertexData(),
			mDataProvider->getVertexStride(),
			mDataProvider->getVertexCount(),
			mDataProvider->getIndexData(),
			mDataProvider->getIndexWidth(),
			mDataProvider->getIndexCount(),
			mDataProvider->getRenderCommands()
		);
	}
}