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

		float mPointSize;

		VertexOptions mVertexOptions;

		mpp::mesh::Vertex::DataType mPositionType;

		mpp::mesh::Vertex::DataType mColourType;

		int mIndexWidth;

	private:

		void createImpl();

		int setIndices(uint32* ptr, uint32 base);

		void setMinimumCount(int count);

		void createMeshSpecification(mesh::Primitive::Type primitiveType);

		void createMesh(Mesh* mesh, size_t vertexCount, size_t bufferSize, std::shared_ptr<const int8> dataPtr);

	public:

		CircleBatch(std::string const& name,
			VertexOptions vertexOptions,
			mpp::mesh::Vertex::DataType positionType,
			mpp::mesh::Vertex::DataType colourType,
			float maxRadius,
			float borderSize,
			int indexWidth,
			uint32 initialCount,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		void finishUpdate(int count, bool updateTexCoords);

		int getPrimitiveCount() const;

		int getVertexCount(int primitiveCount);

		void setPointSize(float size);

		float getPointSize() const;

		bool usingPointSprites() const;
	};

}
