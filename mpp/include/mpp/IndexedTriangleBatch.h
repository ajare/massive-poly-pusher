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
			ResourcePtr texture,
			int indexWidth,
			size_t initialCapacity,
			VertexCountFunction vertexCountFn,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		int getVertexCount(int primitiveCount) const;

		uint8* getIndexData();
	};
}
#pragma once
