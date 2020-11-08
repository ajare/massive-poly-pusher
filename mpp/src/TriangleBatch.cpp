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
		ResourcePtr texture,
		size_t initialCapacity,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Batch(name, initialCapacity, VertexShader2dTemplate, FragmentShader2dTemplate, "tris", options.colourAttrib, options.useDiffuse, renderSystem, resourceMgr)
		, mOptions(options)
		, mTexture(texture)
	{
		if (mTexture && mOptions.texcoordAttrib.dataType == mesh::Vertex::DataType::None)
		{
			THROW_MPP("Must specify a texcoord type when using a texture.", __LINE__, __FILE__, __func__);
		}
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
		auto dynamicLayout = mSpecification.createVertexBufferAttributeLayout(false);
		mesh::VertexBufferAttributeLayout* staticLayout{ nullptr };

		// Position
		dynamicLayout->createAttribute(mesh::Vertex::Component::Position2, mOptions.positionType, false);

		// Texture coords
		if (usingTexture())
		{
			mesh::VertexBufferAttributeLayout* texcoordLayout{ nullptr };
			if (mOptions.texcoordAttrib.fixedValues)
			{
				if (!staticLayout)
				{
					staticLayout = mSpecification.createVertexBufferAttributeLayout(true);
				}

				texcoordLayout = staticLayout;
			}
			else
			{
				texcoordLayout = dynamicLayout;
			}

			texcoordLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mOptions.texcoordAttrib.dataType, false);
		}

		// Colour
		if (mOptions.colourAttrib.dataType != mesh::Vertex::DataType::None)
		{
			mesh::VertexBufferAttributeLayout* colourLayout{ nullptr };
			if (mOptions.colourAttrib.fixedValues)
			{
				if (!staticLayout)
				{
					staticLayout = mSpecification.createVertexBufferAttributeLayout(true);
				}

				colourLayout = staticLayout;
			}
			else
			{
				colourLayout = dynamicLayout;
			}

			colourLayout->createAttribute(mesh::Vertex::Component::Colour4, mOptions.colourAttrib.dataType, mesh::Vertex::isDataTypeNormalisable(mOptions.colourAttrib.dataType));
		}
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

		uint32_t flags = MPP_PROGRAM_TAGS_PRIM_TRIANGLES
			| (usingTexture() ? MPP_PROGRAM_TAGS_TEXTURE1 : 0)
			| (usingDiffuse() ? MPP_PROGRAM_TAGS_DIFFUSE : 0);

		auto materialResource = createMaterial(getName() + "_TriBatch", mTexture, flags);
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
			auto const& layout = mSpecification.getVertexBufferAttributeLayout(i);
			createVertexBuffer(i, mesh, vertexCount, layout.isStatic());
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

	bool TriangleBatch::usingTexture() const
	{
		return mTexture != nullptr;
	}

	bool TriangleBatch::positionFixed() const
	{
		return false;
	}

	bool TriangleBatch::texcoordsFixed() const
	{
		return mOptions.texcoordAttrib.fixedValues;
	}

	bool TriangleBatch::colourFixed() const
	{
		return mOptions.colourAttrib.fixedValues;
	}
}