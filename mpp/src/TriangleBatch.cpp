#include <cmath>

#include "utils/MemTracker.h"

#include "mpp/TriangleBatch.h"
#include "mpp/DefaultShaders.h"
#include "mpp/ProgrammaticMaterialStream.h"
#include "mpp/ResourceManager.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{
	using namespace mesh;

	/*
	 * Constructor.
	 *
	 */
	TriangleBatch::TriangleBatch(string const& name,
		mpp::mesh::Vertex::DataType positionType,
		mpp::mesh::Vertex::DataType texcoordType,
		mpp::mesh::Vertex::DataType colourType,
		size_t initialCapacity,
		string const& texture,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Batch(name, initialCapacity, VertexShader2dTemplate, FragmentShader2dTemplate, "tris", renderSystem, resourceMgr)
		, mPositionType(positionType)
		, mTexcoordType(texcoordType)
		, mColourType(colourType)
		, mTexture(texture)
	{
	}

	bool TriangleBatch::indexedVertices() const
	{
		return false;
	}

	void TriangleBatch::createMeshSpecification(mesh::Primitive::Type primitiveType)
	{
		mSpecification = mesh::MeshSpecification(primitiveType);
		auto layout = mSpecification.createVertexBufferAttributeLayout();

		layout->createAttribute(mesh::Vertex::Component::Position2, mPositionType, false);
		layout->createAttribute(mesh::Vertex::Component::TexCoord2, mTexcoordType, false);
		layout->createAttribute(mesh::Vertex::Component::Colour4, mColourType, true);
	}

	/*
	 * Create the data required.
	 *
	 */
	void TriangleBatch::createImpl()
	{
		auto primitiveType = mesh::Primitive::Type::Triangles;
		int primitiveCount = getPrimitiveCount(getCapacity());
		auto storageType = mesh::VertexBufferStorageType::Dynamic;

		createMeshSpecification(primitiveType);
		auto materialResource = createMaterial(getName() + "_TriBatch", mTexture, MPP_PROGRAM_TAGS_PRIM_TRIANGLES);
		int vertexCount = getVertexCount(primitiveCount);

		auto mesh = new Mesh(
			getRenderSystem(),
			getName(),
			materialResource,
			primitiveType,
			primitiveCount,
			storageType);

		auto bufferSize = mSpecification.getVertexBufferAttributeLayout(0).getVertexSize();
		int8* data = new int8[vertexCount * bufferSize];
		shared_ptr<const int8> dataPtr(data, [](int8*p) { delete[] p; });

		createMesh(mesh, vertexCount, bufferSize, dataPtr);
	}

	void TriangleBatch::finishUpdate(int count, bool updateTexCoords)
	{
		mCurCount = count;

		mpp::VertexBuffer* vertexBuffer0 = mMeshes[0]->getVertexBuffer(0);
		vertexBuffer0->mapBufferData(getVertexCount(count));
		mMeshes[0]->setNumPrimitives(count);
	}

	int TriangleBatch::getPrimitiveCount(int objectCount) const
	{
		return objectCount;
	}

	/*
	 * Get the number of vertices required, given the number of primitives.
	 *
	 */
	int TriangleBatch::getVertexCount(int primitiveCount)
	{
		return primitiveCount * 3;
	}
}