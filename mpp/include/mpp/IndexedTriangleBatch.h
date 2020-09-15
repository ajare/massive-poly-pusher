#pragma once

#include <vector>

#include "mpp/TriangleBatch.h"

namespace mpp
{
	class _MPPAPI IndexedTriangleBatch : public TriangleBatch
	{
	public:

		typedef std::function<int(int)> VertexCountFunction;

	private:

		VertexCountFunction mVertexCountFn;

		size_t mIndexWidth;

	private:

		bool indexedVertices() const;

		void createImpl();

		void createIndexData(std::vector<uint8>& data, uint32_t start, size_t count);

	public:

		IndexedTriangleBatch(std::string const& name,
			mpp::mesh::Vertex::DataType positionType,
			mpp::mesh::Vertex::DataType texcoordType,
			mpp::mesh::Vertex::DataType colourType,
			int indexWidth,
			size_t initialCapacity,
			std::string const& texture,
			VertexCountFunction vertexCountFn,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		void finishUpdate(int count, bool updateTexCoords);

		int getVertexCount(int primitiveCount);

		uint8* getIndexData();
	};
}
#pragma once
