#include <cmath>

#include "utils/MemTracker.h"

#include "mpp/CircleBatch.h"
#include "mpp/DefaultShaders.h"
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
	CircleBatch::CircleBatch(string const& name,
		CircleBatchOptions const& options,
		size_t indexWidth,
		float maxRadius,
		float borderSize,
		bool antiAlias,
		size_t initialCapacity,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Batch(
			name, 
			initialCapacity, 
			VertexShader2dCircle, 
			antiAlias ? FragmentShader2dCircleAntialiased : FragmentShader2dCircle, 
			"circle",
			options.colourAttrib,
			options.useDiffuse,
			renderSystem, 
			resourceMgr)
		, mOptions(options)
		, mRadius(maxRadius)
		, mBorderSize(borderSize)
		, mAntiAlias(antiAlias)
		, mIndexWidth(indexWidth)
	{
		// Set vertex options
		float size = mRadius * 2;
		Caps const& caps = getRenderSystem()->getCaps();
		bool canUsePointSprites = caps.pointSizeRange[0] < size && caps.pointSizeRange[1] > size;

		if (options.vertexOptions == CircleBatchOptions::VertexOptions::Points && !canUsePointSprites)
		{
			THROW_MPP("Cannot use point sprites for this CircleBatch.", __LINE__, __FILE__, __func__);
		}

		if (canUsePointSprites && options.vertexOptions != CircleBatchOptions::VertexOptions::Triangles)
		{
			mOptions.vertexOptions = CircleBatchOptions::VertexOptions::Points;
		}
		else
		{
			mOptions.vertexOptions = CircleBatchOptions::VertexOptions::Triangles;
		}
	}

	mesh::Primitive::Type CircleBatch::getPrimitiveType() const
	{
		return usingPointSprites() ? mesh::Primitive::Type::Points : mesh::Primitive::Type::Triangles;
	}

	bool CircleBatch::indexedVertices() const
	{
		return !usingPointSprites();
	}

	/*
	 * Create indices for an object.
	 *
	 */
	void CircleBatch::createIndexData(vector<uint8>& data, uint32_t start, size_t count)
	{
		size_t vertexSize{ 6 * (mIndexWidth / 8) };
		data.resize(count * vertexSize);

		uint32* ptr = (uint32*)&data[start * vertexSize]; // Indices will be 16 or 32-bit, so use 32 to cover both
		int indexBytes = mIndexWidth / 8;

		for (uint32_t i = start; i < count; ++i)
		{
			if (indexBytes == 2)
			{
				*ptr = (i * 4 + 0) + ((i * 4 + 1) << 16); ptr++;
				*ptr = (i * 4 + 2) + ((i * 4 + 2) << 16); ptr++;
				*ptr = (i * 4 + 3) + ((i * 4 + 0) << 16); ptr++;
			}
			else if (indexBytes == 4)
			{
				*ptr = i * 4 + 0; ptr++;
				*ptr = i * 4 + 1; ptr++;
				*ptr = i * 4 + 2; ptr++;
				*ptr = i * 4 + 2; ptr++;
				*ptr = i * 4 + 3; ptr++;
				*ptr = i * 4 + 0; ptr++;
			}
		}
	}

	void CircleBatch::createMeshSpecification(mesh::Primitive::Type primitiveType)
	{
		mSpecification = mesh::MeshSpecification(primitiveType);
		auto layout = mSpecification.createVertexBufferAttributeLayout(false);

		if (usingPointSprites())
		{
			layout->createAttribute(mesh::Vertex::Component::Position2, mOptions.positionType, false);
		}
		else
		{
			layout->createAttribute(mesh::Vertex::Component::Position4, mOptions.positionType, false);
		}

		layout->createAttribute(mesh::Vertex::Component::UserDefined4, "OPTIONS", mesh::Vertex::DataType::Float, false);
		layout->createAttribute(mesh::Vertex::Component::UserDefined4, "BORDERCOLOUR", getColourAttribute().dataType, true);
		layout->createAttribute(mesh::Vertex::Component::UserDefined4, "INNERCOLOUR", getColourAttribute().dataType, true);
	}

	/*
	 * Create the data required.
	 *
	 */
	void CircleBatch::createImpl()
	{
		float size = mRadius * 2;

		auto primitiveType = getPrimitiveType();
		int primitiveCount = getPrimitiveCount(getCapacity());
		auto storageType = mesh::VertexBufferStorageType::Dynamic;

		createMeshSpecification(primitiveType);
		auto materialResource = createMaterial(getName() + "_CircleBatch", "__mpp_tex_none__", usingPointSprites() ? MPP_PROGRAM_TAGS_PRIM_POINTS : MPP_PROGRAM_TAGS_PRIM_TRIANGLES);
		int vertexCount = getVertexCount(primitiveCount);

		Mesh* mesh{ nullptr };
		if (indexedVertices())
		{
			vector<uint8> indices;
			createIndexData(indices, 0, getCapacity());

			mesh = new Mesh(
				getRenderSystem(),
				getName(),
				materialResource,
				primitiveType,
				primitiveCount,
				mIndexWidth,
				indices,
				storageType,
				size);
		}
		else
		{
			mesh = new Mesh(
				getRenderSystem(), 
				getName(), 
				materialResource, 
				primitiveType, 
				primitiveCount, 
				storageType, 
				size);
		}

		for (int i = 0; i < mSpecification.getNumVertexBufferAttributeLayouts(); ++i)
		{
			createVertexBuffer(i, mesh, vertexCount, false);
		}

		setSpecificationPointers(mesh);
		mMeshes.push_back(mesh);
	}

	size_t CircleBatch::getPrimitiveCount(size_t objectCount) const
	{
		return objectCount * (usingPointSprites() ? 1 : 2);
	}

	size_t CircleBatch::getVertexCount(size_t primitiveCount)
	{
		// If using triangles, we expect primitiveCount to be a multiple of 2
		return primitiveCount * (usingPointSprites() ? 1 : 2);
	}

	bool CircleBatch::usingPointSprites() const
	{
		return mOptions.vertexOptions == CircleBatchOptions::VertexOptions::Points;
	}

	float CircleBatch::getRadius() const
	{
		return mRadius;
	}

}