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
		TriangleBatchOptions const& options,
		size_t initialCapacity,
		string const& texture,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Batch(name, initialCapacity, VertexShader2dTemplate, FragmentShader2dTemplate, "tris", options.colourAttrib, options.useDiffuse, renderSystem, resourceMgr)
		, mOptions(options)
		, mTexture(texture)
	{
	}

	bool TriangleBatch::indexedVertices() const
	{
		return false;
	}

	mesh::Primitive::Type TriangleBatch::getPrimitiveType() const
	{
		return mesh::Primitive::Type::Triangles;
	}

	void TriangleBatch::createMeshSpecification(mesh::Primitive::Type primitiveType)
	{
		mSpecification = mesh::MeshSpecification(primitiveType);
		auto layout = mSpecification.createVertexBufferAttributeLayout(false);

		layout->createAttribute(mesh::Vertex::Component::Position2, mOptions.positionType, false);
		layout->createAttribute(mesh::Vertex::Component::TexCoord2, mOptions.texcoordAttrib.dataType, false);
		layout->createAttribute(mesh::Vertex::Component::Colour4, getColourAttribute().dataType, true);
	}

	/*
	 * Create the data required.
	 *
	 */
	void TriangleBatch::createImpl()
	{
		auto primitiveType = getPrimitiveType();
		int primitiveCount = getPrimitiveCount(getCapacity());

		createMeshSpecification(primitiveType);
		auto materialResource = createMaterial(getName() + "_TriBatch", mTexture, MPP_PROGRAM_TAGS_PRIM_TRIANGLES);
		int vertexCount = getVertexCount(primitiveCount);

		auto mesh = new Mesh(
			getRenderSystem(),
			getName(),
			materialResource,
			primitiveType,
			primitiveCount,
			mesh::VertexBufferStorageType::Dynamic);

		for (int i = 0; i < mSpecification.getNumVertexBufferAttributeLayouts(); ++i)
		{
			createVertexBuffer(i, mesh, vertexCount, false);
		}

		setSpecificationPointers(mesh);
		mMeshes.push_back(mesh);
	}
	
	size_t TriangleBatch::getPrimitiveCount(size_t objectCount) const
	{
		return objectCount;
	}

	/*
	 * Get the number of vertices required, given the number of primitives.
	 *
	 */
	size_t TriangleBatch::getVertexCount(size_t primitiveCount) const
	{
		return primitiveCount * 3;
	}
}