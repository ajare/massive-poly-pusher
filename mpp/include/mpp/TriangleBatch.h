#pragma once

#include <functional>
#include <vector>

#include "mpp/Batch.h"

namespace mpp
{

	struct TriangleBatchOptions
	{
		enum class Dimension
		{
			P2D,
			P3D
		};

		Dimension dimension{ Dimension::P2D };
		mpp::mesh::Vertex::DataType positionType;
		BatchVertexAttribute texcoordAttrib;
		BatchVertexAttribute colourAttrib;
		bool useDiffuse;
	};

	class _MPPAPI TriangleBatch : public Batch
	{
		TriangleBatchOptions mOptions;

	protected:

		ResourcePtr mTexture;

	private:

		void createImpl();

		bool indexedVertices() const;

	protected:

		mesh::Primitive::Type getPrimitiveType() const;

		void createMeshSpecification(mesh::Primitive::Type primitiveType);

	public:

		TriangleBatch(std::string const& name,
			TriangleBatchOptions const& options,
			ResourcePtr texture,
			size_t initialCapacity,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		size_t getPrimitiveCount(size_t objectCount) const;

		size_t getVertexCount(size_t primitiveCount) const;

		bool usingTexture() const;

		bool positionFixed() const;

		bool texcoordsFixed() const;

		bool colourFixed() const;

	};
}
