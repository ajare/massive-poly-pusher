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

		int mIndexWidth;

	private:

		void setMinimumCount(int count);

		void resizeIndexData(int count);

	protected:

		void postCreate();

	public:

		IndexedTriangleBatch(std::string const& name,
			mpp::mesh::Vertex::DataType positionType,
			mpp::mesh::Vertex::DataType texcoordType,
			ColourOptions colourOptions,
			bool useDiffuseColour,
			ResourcePtr program,
			ResourcePtr texture,
			int indexWidth,
			VertexCountFunction vertexCountFn,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		uint8* getIndexData();

		int getVertexCount(int primitiveCount);

	};
}
#pragma once
