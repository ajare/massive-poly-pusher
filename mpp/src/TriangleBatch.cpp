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
		ResourcePtr textureOrMaterial,
		size_t initialCapacity,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Batch(name, initialCapacity, VertexShader2dTemplate, FragmentShader2dTemplate, "tris", options.colourAttrib, options.useDiffuse, renderSystem, resourceMgr)
		, mOptions(options)
		, mTextureOrMaterial(textureOrMaterial)
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

	mesh::MeshSpecification TriangleBatch::createMeshSpecification(mesh::Primitive::Type primitiveType)
	{
		auto meshSpec = mesh::MeshSpecification(primitiveType);
		auto dynamicLayout = meshSpec.createVertexBufferAttributeLayout(false);
		mesh::VertexBufferAttributeLayout* staticLayout{ nullptr };

		// Position
		if (mOptions.dimension == TriangleBatchOptions::Dimension::P2D)
		{
			dynamicLayout->createAttribute(mesh::Vertex::Component::Position2, mOptions.positionType, false);
		}
		else
		{
			dynamicLayout->createAttribute(mesh::Vertex::Component::Position3, mOptions.positionType, false);
			dynamicLayout->createAttribute(mesh::Vertex::Component::Normal3, mOptions.positionType, false);
		}

		// Texture coords
		if (usingTexture())
		{
			mesh::VertexBufferAttributeLayout* texcoordLayout{ nullptr };
			if (mOptions.texcoordAttrib.fixedValues)
			{
				if (!staticLayout)
				{
					staticLayout = meshSpec.createVertexBufferAttributeLayout(true);
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
					staticLayout = meshSpec.createVertexBufferAttributeLayout(true);
				}

				colourLayout = staticLayout;
			}
			else
			{
				colourLayout = dynamicLayout;
			}

			colourLayout->createAttribute(mesh::Vertex::Component::Colour4, mOptions.colourAttrib.dataType, mesh::Vertex::isDataTypeNormalisable(mOptions.colourAttrib.dataType));
		}

		return meshSpec;
	}

	/*
	 * Create the data required.
	 *
	 */
	void TriangleBatch::createImpl()
	{
		auto primitiveType = getPrimitiveType();
		int primitiveCount = getPrimitiveCount(getCapacity());

		if (mOptions.specifyMaterial)
		{
			auto mat = static_cast<Material*>(mTextureOrMaterial.get());
			auto prog = static_cast<Program*>(mat->getProgram().get());
			mSpecification = prog->getMeshSpecification();

			auto targetSpec = createMeshSpecification(primitiveType);
			if (mSpecification != targetSpec)
			{
				THROW_MPP("MeshSpecification in Material is not compatible with TriangleBatch", __LINE__, __FILE__, __func__);
			}
		}
		else
		{
			mSpecification = createMeshSpecification(primitiveType);
		}

		uint32_t flags = MPP_PROGRAM_TAGS_PRIM_TRIANGLES
			| (usingTexture() ? MPP_PROGRAM_TAGS_TEXTURE1 : 0)
			| (usingDiffuse() ? MPP_PROGRAM_TAGS_DIFFUSE : 0);

		auto materialResource = mOptions.specifyMaterial ? mTextureOrMaterial
			: createMaterial(getName() + "_TriBatch", mTextureOrMaterial, flags, mOptions.dimension == TriangleBatchOptions::Dimension::P2D);
		
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
		if (mOptions.specifyMaterial)
		{
			auto mat = static_cast<Material*>(mTextureOrMaterial.get());
			return mat->getNumTextures() > 0;
		}
		else
		{
			return mTextureOrMaterial != nullptr;
		}
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