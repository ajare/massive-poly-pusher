#include <cmath>

#include "mpp/IndexedTriangleBatch.h"
#include "mpp/ProgrammaticMaterialStream.h"
#include "mpp/ResourceManager.h"

using namespace std;

namespace mpp
{
	using namespace mesh;

	/*
	 * Constructor.
	 *
	 */
	IndexedTriangleBatch::IndexedTriangleBatch(string const& name,
		mpp::mesh::Vertex::DataType positionType,
		mpp::mesh::Vertex::DataType texcoordType,
		ColourOptions colourOptions,
		bool useDiffuseColour,
		ResourcePtr program,
		ResourcePtr texture,
		int indexWidth,
		VertexCountFunction vertexCountFn,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: TriangleBatch(name, positionType, texcoordType, colourOptions, useDiffuseColour, program, texture, 0, renderSystem, resourceMgr)
		, mIndexWidth(indexWidth)
		, mVertexCountFn(vertexCountFn)
	{
	}

	/*
	 * Get the number of vertices required, given the number of primitives.
	 *
	 */
	int IndexedTriangleBatch::getVertexCount(int primitiveCount)
	{
		return mVertexCountFn(primitiveCount);
	}

	/*
	 * Set index data size
	 *
	 */
	void IndexedTriangleBatch::resizeIndexData(int count)
	{
		const int indexStride = 3 * (mIndexWidth / 8);

		vector<uint8>& indexData = mMeshes[0]->getIndexData();
		indexData.resize(count * indexStride, 0);
	}

	/*
	 * Create the data required.
	 *
	 */
	void IndexedTriangleBatch::postCreate()
	{
		const int indexStride = 3 * (mIndexWidth / 8);

		vector<uint8> indexData(mMaxCount * indexStride, 0);
		mMeshes[0]->setIndexData(indexData, mIndexWidth);
	}

	/*
	 * Get index data
	 *
	 */
	uint8* IndexedTriangleBatch::getIndexData()
	{
		auto& indexData = mMeshes[0]->getIndexData();
		return (uint8*)&(indexData[0]);
	}

	void IndexedTriangleBatch::setMinimumCount(int count)
	{
		// For triangles, because strips do not have a linear relationship
		// between vertex and triangle count, check vertex count, not primitive
		// count
		mpp::VertexBuffer* vertexBuffer0 = mMeshes[0]->getVertexBuffer(0);
		auto& data = vertexBuffer0->getBufferData();

		int currentVertices = data.size() / mMainBufferStride;
		int requiredVertices = getVertexCount(count);

		if (requiredVertices > currentVertices)
		{
			// Main data (position/colour)
			data.resize(requiredVertices * mMainBufferStride);

			// Texcoord data
			mpp::VertexBuffer* vertexBuffer1 = nullptr;
			if (mUseTexCoords)
			{
				vertexBuffer1 = mMeshes[0]->getVertexBuffer(1);
				auto& data = vertexBuffer1->getBufferData();

				data.resize(requiredVertices * mTexCoordBufferStride);
			}

			// Index data
			resizeIndexData(count);

			mMaxCount = count;
			setSpecificationPointers(vertexBuffer0, vertexBuffer1);
		}
	}

}