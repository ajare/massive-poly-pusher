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
		TriangleBatchOptions const& options,
		ResourcePtr texture,
		int indexWidth,
		size_t initialCapacity,
		VertexCountFunction vertexCountFn,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: TriangleBatch(name, options, texture, initialCapacity, renderSystem, resourceMgr)
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

		uint32 flags = MPP_PROGRAM_TAGS_PRIM_TRIANGLES
			| (usingTexture() ? MPP_PROGRAM_TAGS_TEXTURE1 : 0)
			| (usingDiffuse() ? MPP_PROGRAM_TAGS_DIFFUSE : 0);

		auto materialResource = createMaterial(getName() + "_TriBatch", mTexture->getName(), flags);
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

		for (int i = 0; i < mSpecification.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto const& layout = mSpecification.getVertexBufferAttributeLayout(i);
			createVertexBuffer(i, mesh, vertexCount, layout.isStatic());
		}

		setSpecificationPointers(mesh);
		mMeshes.push_back(mesh);
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
	int IndexedTriangleBatch::getVertexCount(int primitiveCount) const
	{
		return mVertexCountFn(primitiveCount);
	}

}