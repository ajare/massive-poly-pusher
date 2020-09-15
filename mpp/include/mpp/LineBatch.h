#pragma once
#pragma once

#include <vector>

#include "mpp/Batch.h"

namespace mpp
{
	class _MPPAPI LineBatch : public Batch2
	{
		mpp::mesh::Vertex::DataType mPositionType;

		mpp::mesh::Vertex::DataType mColourType;

	private:

		void createImpl();

		void createMeshSpecification(mesh::Primitive::Type primitiveType);

		bool indexedVertices() const;

	public:

		LineBatch(std::string const& name,
			mpp::mesh::Vertex::DataType positionType,
			mpp::mesh::Vertex::DataType colourType,
			size_t initialCapacity,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		void finishUpdate(int count, bool updateTexCoords);

		int getPrimitiveCount(int objectCount) const;

		int getVertexCount(int primitiveCount);
	};
}
