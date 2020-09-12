#pragma once

#include <vector>

#include "mpp/Batch.h"

namespace mpp
{
	class _MPPAPI CircleBatch : public Batch2
	{
	public:

		enum class VertexOptions
		{
			Auto,
			Points,
			Triangles
		};

	private:

		float mRadius;

		float mBorderSize;

		VertexOptions mVertexOptions;

		mpp::mesh::Vertex::DataType mPositionType;

		mpp::mesh::Vertex::DataType mColourType;

		size_t mIndexWidth;

	private:

		bool indexedVertices() const;

		void createMeshSpecification(mesh::Primitive::Type primitiveType);

		void createImpl();

		void createIndexData(std::vector<uint8>& data, uint32_t start, size_t count);

	public:

		CircleBatch(std::string const& name,
			VertexOptions vertexOptions,
			mpp::mesh::Vertex::DataType positionType,
			mpp::mesh::Vertex::DataType colourType,
			float maxRadius,
			float borderSize,
			size_t indexWidth,
			uint32 initialCount,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		void finishUpdate(int count, bool updateTexCoords);

		int getPrimitiveCount(int objectCount) const;

		int getVertexCount(int primitiveCount);

		bool usingPointSprites() const;

		float getRadius() const;
	};

}
