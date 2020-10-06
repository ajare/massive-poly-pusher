#pragma once

#include <functional>
#include <vector>

#include "mpp/Batch.h"

namespace mpp
{

	struct TriangleBatchOptions
	{
		mpp::mesh::Vertex::DataType positionType;
		BatchVertexAttribute texcoordAttrib;
		BatchVertexAttribute colourAttrib;
		bool useDiffuse;
	};

	class _MPPAPI TriangleBatch : public Batch
	{
		TriangleBatchOptions mOptions;

	protected:

		std::string mTexture;

	private:

		void createImpl();

		bool indexedVertices() const;

	protected:

		mesh::Primitive::Type getPrimitiveType() const;

		void createMeshSpecification(mesh::Primitive::Type primitiveType);

	public:

		TriangleBatch(std::string const& name,
			TriangleBatchOptions const& options,
			size_t initialCapacity,
			std::string const& texture,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		size_t getPrimitiveCount(size_t objectCount) const;

		size_t getVertexCount(size_t primitiveCount) const;
	};
}
