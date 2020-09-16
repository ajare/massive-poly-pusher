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
		LineBatchOptions const& options,
		size_t initialCapacity,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Batch(name, initialCapacity, VertexShader2dTemplate, FragmentShader2dTemplate, "line", renderSystem, resourceMgr)
		, mOptions(options)
	{
	}

	/*
	 * Create mesh specification.
	 *
	 */
	void LineBatch::createMeshSpecification(mesh::Primitive::Type primitiveType)
	{
		mSpecification = mesh::MeshSpecification(primitiveType);
		auto layout = mSpecification.createVertexBufferAttributeLayout();

		layout->createAttribute(mesh::Vertex::Component::Position2, mOptions.positionType, false);

		if (mOptions.colourType != mesh::Vertex::DataType::None)
		{
			layout->createAttribute(mesh::Vertex::Component::Colour4, mOptions.colourType, true);
		}
	}

	/*
	 * We don't use indexed vertices for line batches, even though we could.
	 *
	 */
	bool LineBatch::indexedVertices() const
	{
		return false;
	}

	mesh::Primitive::Type LineBatch::getPrimitiveType() const
	{
		return mesh::Primitive::Type::Lines;
	}

	/*
	 * Create the data required.
	 *
	 */
	void LineBatch::createImpl()
	{
		auto primitiveType = getPrimitiveType();
		int primitiveCount = getPrimitiveCount(getCapacity());

		createMeshSpecification(primitiveType);

		uint32 flags = MPP_PROGRAM_TAGS_PRIM_LINES
			| (mOptions.useDiffuse ? MPP_PROGRAM_TAGS_DIFFUSE : 0);

		auto materialResource = createMaterial(getName() + "_LineBatch", "__mpp_tex_none__", flags);
		int vertexCount = getVertexCount(primitiveCount);

		auto mesh = new Mesh(
			getRenderSystem(),
			getName(),
			materialResource,
			primitiveType,
			primitiveCount,
			mesh::VertexBufferStorageType::Dynamic);

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