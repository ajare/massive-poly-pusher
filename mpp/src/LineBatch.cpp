#include <cmath>

#include "utils/MemTracker.h"

#include "mpp/LineBatch.h"
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
	LineBatch::LineBatch(string const& name,
		mpp::mesh::Vertex::DataType positionType,
		mpp::mesh::Vertex::DataType colourType,
		size_t initialCapacity,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Batch(name, initialCapacity, VertexShader2dTemplate, FragmentShader2dTemplate, "line", renderSystem, resourceMgr)
		, mPositionType(positionType)
		, mColourType(colourType)
	{
	}

	void LineBatch::createMeshSpecification(mesh::Primitive::Type primitiveType)
	{
		mSpecification = mesh::MeshSpecification(primitiveType);
		auto layout = mSpecification.createVertexBufferAttributeLayout();

		layout->createAttribute(mesh::Vertex::Component::Position2, mPositionType, false);
		layout->createAttribute(mesh::Vertex::Component::Colour4, mColourType, true);
	}

	bool LineBatch::indexedVertices() const
	{
		return false;
	}

	/*
	 * Create the data required.
	 *
	 */
	void LineBatch::createImpl()
	{
		auto primitiveType = mesh::Primitive::Type::Lines;
		int primitiveCount = getPrimitiveCount(getCapacity());
		auto storageType = mesh::VertexBufferStorageType::Dynamic;

		createMeshSpecification(primitiveType);
		auto materialResource = createMaterial(getName() + "_LineBatch", "__mpp_tex_none__", MPP_PROGRAM_TAGS_PRIM_LINES);
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

	void LineBatch::finishUpdate(int count, bool updateTexCoords)
	{
		mCurCount = count;

		auto numPrimitives = getPrimitiveCount(count);
		for (int i = 0; i < mMeshes[0]->getNumVertexBuffers(); ++i)
		{
			auto vertexBuffer = mMeshes[0]->getVertexBuffer(i);
			vertexBuffer->mapBufferData(getVertexCount(numPrimitives));
		}

		mMeshes[0]->setNumPrimitives(numPrimitives);
	}

	int LineBatch::getPrimitiveCount(int objectCount) const
	{
		return objectCount;
	}

	/*
	 * Get the number of vertices required, given the number of primitives.
	 *
	 */
	int LineBatch::getVertexCount(int primitiveCount)
	{
		return primitiveCount * 2;
	}
}