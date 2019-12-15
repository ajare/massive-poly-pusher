#pragma once
#pragma once

#include <vector>

#include "mpp/Batch.h"

namespace mpp
{
	class _MPPAPI LineBatch : public Batch
	{
		mpp::mesh::Vertex::DataType mPositionType;

	private:

		void createImpl();

		void writeVertex(float x, float y, int8** mainPtr);

		void setSpecificationPointers(VertexBuffer* mainBuffer, VertexBuffer* texCoordBuffer);

		void setMinimumCount(int count);

		void setMainBufferStride();

		void setTexCoordBufferStride();

		int getVertexCount(int primitiveCount);

	public:

		LineBatch(std::string const& name,
			mpp::mesh::Vertex::DataType positionType,
			ColourOptions colourOptions,
			bool useDiffuseColour,
			uint32 initialCount,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		void finishUpdate(int count, bool updateTexCoords);
	};
}
