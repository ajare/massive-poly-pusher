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
			TriangleBatchOptions const& options,
			int indexWidth,
			size_t initialCapacity,
			std::string const& texture,
			VertexCountFunction vertexCountFn,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		int getVertexCount(int primitiveCount);

		uint8* getIndexData();
	};
}
#pragma once
