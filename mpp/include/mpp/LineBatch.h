#pragma once
#pragma once

#include <vector>

#include "mpp/Batch.h"

namespace mpp
{
	struct LineBatchOptions
	{
		mpp::mesh::Vertex::DataType positionType;
		mpp::mesh::Vertex::DataType colourType;
		bool useDiffuse;
	};

	class _MPPAPI LineBatch : public Batch
	{
		LineBatchOptions mOptions;

	private:

		void createImpl();

		void createMeshSpecification(mesh::Primitive::Type primitiveType);

		bool indexedVertices() const;

	protected:

		mesh::Primitive::Type getPrimitiveType() const;

	public:

		LineBatch(std::string const& name,
			LineBatchOptions const& options,
			size_t initialCapacity,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		void finishUpdate(int count, bool updateTexCoords);

		int getPrimitiveCount(int objectCount) const;

		int getVertexCount(int primitiveCount);
	};
}
