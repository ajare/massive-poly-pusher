#pragma once

#include <vector>

#include "mpp/QuadBatch.h"

namespace mpp
{
	class _MPPAPI CircleBatch : public QuadBatch
	{
		float mRadius;

		float mBorderSize;

		mpp::mesh::Vertex::DataType mColourType;

		int mNormalOffset;

		char* mNormalData;

	private:

		void createImpl();

		void setMainBufferStride();

		void setTexCoordBufferStride();

		void setSpecificationPointers(VertexBuffer* mainBuffer, VertexBuffer* texCoordBuffer);

	public:

		CircleBatch(std::string const& name,
			VertexOptions vertexOptions,
			mpp::mesh::Vertex::DataType positionType,
			mpp::mesh::Vertex::DataType texcoordType,
			mpp::mesh::Vertex::DataType colourType,
			float maxRadius,
			float borderSize,
			int indexWidth,
			uint32 initialCount,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		char* getNormalData();

		size_t getNormalStride() const;

	};

}
