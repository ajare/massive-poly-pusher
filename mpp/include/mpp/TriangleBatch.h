#pragma once

#include <functional>
#include <vector>

#include "mpp/Batch.h"

namespace mpp
{

	class _MPPAPI TriangleBatch : public Batch2
	{
	protected:

		mpp::mesh::Vertex::DataType mPositionType;

		mpp::mesh::Vertex::DataType mTexcoordType;

		mpp::mesh::Vertex::DataType mColourType;

		std::string mTexture;

	private:

		void createImpl();

		bool indexedVertices() const;

	protected:

		void createMeshSpecification(mesh::Primitive::Type primitiveType);

	public:

		TriangleBatch(std::string const& name,
			mpp::mesh::Vertex::DataType positionType,
			mpp::mesh::Vertex::DataType texcoordType,
			mpp::mesh::Vertex::DataType colourType,
			size_t initialCapacity,
			std::string const& texture,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		void finishUpdate(int count, bool updateTexCoords);

		int getPrimitiveCount(int objectCount) const;

		int getVertexCount(int primitiveCount);
	};
}
