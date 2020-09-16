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
		mpp::mesh::Vertex::DataType colourType,
		int indexWidth,
		size_t initialCapacity,
		string const& texture,
		VertexCountFunction vertexCountFn,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: TriangleBatch(name, positionType, texcoordType, colourType, initialCapacity, texture, renderSystem, resourceMgr) 
		, mIndexWidth(indexWidth)
		, mVertexCountFn(vertexCountFn)
	{
	}

	bool IndexedTriangleBatch::indexedVertices() const
	{
		return true;
	}

	/*
	 * Create the data required.
	 *
	 */
	void IndexedTriangleBatch::createImpl()
	{
		auto primitiveType = getPrimitiveType();
		int primitiveCount = getPrimitiveCount(getCapacity());

		createMeshSpecification(primitiveType);
		auto materialResource = createMaterial(getName() + "_TriBatch", mTexture, MPP_PROGRAM_TAGS_PRIM_TRIANGLES);
		int vertexCount = getVertexCount(primitiveCount);

		const int indexStride = 3 * (mIndexWidth / 8);
		vector<uint8> indices(mMaxCount * indexStride, 0);

		auto mesh = new Mesh(
			getRenderSystem(),
			getName(),
			materialResource,
			primitiveType,
			primitiveCount,
			mIndexWidth,
			indices,
			mesh::VertexBufferStorageType::Dynamic);

		auto bufferSize = mSpecification.getVertexBufferAttributeLayout(0).getVertexSize();
		int8* data = new int8[vertexCount * bufferSize];
		shared_ptr<const int8> dataPtr(data, [](int8*p) { delete[] p; });

		createMesh(mesh, vertexCount, bufferSize, dataPtr);
	}

	void IndexedTriangleBatch::finishUpdate(int count, bool updateTexCoords)
	{
		mCurCount = count;

		mMeshes[0]->mapIndexData(count);

		mpp::VertexBuffer* vertexBuffer0 = mMeshes[0]->getVertexBuffer(0);
		vertexBuffer0->mapBufferData(getVertexCount(count));
		mMeshes[0]->setNumPrimitives(count);
	}

	/*
	 * Create indices for an object.
	 *
	 */
	void IndexedTriangleBatch::createIndexData(vector<uint8>& data, uint32_t start, size_t count)
	{
		size_t vertexSize{ 6 * (mIndexWidth / 8) };
		data.resize(count * vertexSize);
	}

	uint8* IndexedTriangleBatch::getIndexData()
	{
		auto& indexData = mMeshes[0]->getIndexData();
		return (uint8*)&(indexData[0]);
	}

	/*
	 * Get the number of vertices required, given the number of primitives.
	 *
	 */
	int IndexedTriangleBatch::getVertexCount(int primitiveCount)
	{
		return mVertexCountFn(primitiveCount);
	}

}