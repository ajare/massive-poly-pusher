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
		: Batch(name, initialCapacity, VertexShader2dTemplate, FragmentShader2dTemplate, "line", options.colourAttrib, options.useDiffuse, renderSystem, resourceMgr)
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
		auto dynamicLayout = mSpecification.createVertexBufferAttributeLayout(false);

		dynamicLayout->createAttribute(mesh::Vertex::Component::Position2, mOptions.positionType, false);

		if (mOptions.colourAttrib.dataType != mesh::Vertex::DataType::None)
		{
			mesh::VertexBufferAttributeLayout* colourLayout{ dynamicLayout };

			if (mOptions.colourAttrib.fixedValues)
			{
				colourLayout = mSpecification.createVertexBufferAttributeLayout(true);
			}

			colourLayout->createAttribute(mesh::Vertex::Component::Colour4, mOptions.colourAttrib.dataType, true);
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

		uint32_t flags = MPP_PROGRAM_TAGS_PRIM_LINES
			| (usingDiffuse() ? MPP_PROGRAM_TAGS_DIFFUSE : 0);

		auto materialResource = createMaterial(getName() + "_LineBatch", nullptr, flags);
		int vertexCount = getVertexCount(primitiveCount);

		auto mesh = new Mesh(
			getRenderSystem(),
			getName(),
			materialResource,
			primitiveType,
			primitiveCount,
			mesh::VertexBufferStorageType::Dynamic);

		for (size_t i = 0; i < mSpecification.getNumVertexBufferAttributeLayouts(); ++i)
		{
			createVertexBuffer(i, mesh, vertexCount, false);
		}

		setSpecificationPointers(mesh);
		mMeshes.push_back(mesh);
	}

	size_t LineBatch::getPrimitiveCount(size_t objectCount) const
	{
		return objectCount;
	}

	/*
	 * Get the number of vertices required, given the number of primitives.
	 *
	 */
	size_t LineBatch::getVertexCount(size_t primitiveCount) const
	{
		return primitiveCount * 2;
	}


	bool LineBatch::positionFixed() const
	{
		return false;
	}

	bool LineBatch::colourFixed() const
	{
		return mOptions.colourAttrib.fixedValues;
	}

}