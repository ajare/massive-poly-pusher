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
		VertexOptions vertexOptions,
		mpp::mesh::Vertex::DataType positionType,
		mpp::mesh::Vertex::DataType colourType,
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
			renderSystem, 
			resourceMgr)
		, mRadius(maxRadius)
		, mBorderSize(borderSize)
		, mAntiAlias(antiAlias)
		, mVertexOptions(vertexOptions)
		, mPositionType(positionType)
		, mColourType(colourType)
		, mIndexWidth(indexWidth)
	{
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
		auto layout = mSpecification.createVertexBufferAttributeLayout();

		if (usingPointSprites())
		{
			layout->createAttribute(mesh::Vertex::Component::Position2, mPositionType, false);
		}
		else
		{
			layout->createAttribute(mesh::Vertex::Component::Position4, mPositionType, false);
		}

		layout->createAttribute(mesh::Vertex::Component::UserDefined4, "OPTIONS", mesh::Vertex::DataType::Float, false);
		layout->createAttribute(mesh::Vertex::Component::UserDefined4, "BORDERCOLOUR", mColourType, true);
		layout->createAttribute(mesh::Vertex::Component::UserDefined4, "INNERCOLOUR", mColourType, true);
	}

	/*
	 * Create the data required.
	 *
	 */
	void CircleBatch::createImpl()
	{
		//
		// Set up options for this batch
		//
		float size = mRadius * 2;

		// Set vertex options
		Caps const& caps = getRenderSystem()->getCaps();
		bool canUsePointSprites = caps.pointSizeRange[0] < size && caps.pointSizeRange[1] > size;

		if (mVertexOptions == VertexOptions::Points && !canUsePointSprites)
		{
			THROW_MPP("Cannot use point sprites for this CircleBatch.", __LINE__, __FILE__, __func__);
		}

		if (canUsePointSprites && mVertexOptions != VertexOptions::Triangles)
		{
			mVertexOptions = VertexOptions::Points;
		}
		else
		{
			mVertexOptions = VertexOptions::Triangles;
		}

		auto primitiveType = usingPointSprites() ? mesh::Primitive::Type::Points : mesh::Primitive::Type::Triangles;
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

		auto bufferSize = mSpecification.getVertexBufferAttributeLayout(0).getVertexSize();
		int8* data = new int8[vertexCount * bufferSize];
		shared_ptr<const int8> dataPtr(data, [](int8*p) { delete[] p; });

		createMesh(mesh, vertexCount, bufferSize, dataPtr);
	}

	void CircleBatch::finishUpdate(int count, bool updateTexCoords)
	{
		mCurCount = count;

		if (mMeshes[0]->isIndexed())
		{
			mMeshes[0]->mapIndexData(count * (usingPointSprites() ? 1 : 2));
		}

		auto numPrimitives = getPrimitiveCount(count);
		for (int i = 0; i < mMeshes[0]->getNumVertexBuffers(); ++i)
		{
			auto vertexBuffer = mMeshes[0]->getVertexBuffer(i);
			vertexBuffer->mapBufferData(getVertexCount(numPrimitives));
		}

		mMeshes[0]->setNumPrimitives(numPrimitives);
	}

	int CircleBatch::getPrimitiveCount(int objectCount) const
	{
		return objectCount * (usingPointSprites() ? 1 : 2);
	}

	int CircleBatch::getVertexCount(int primitiveCount)
	{
		// If using triangles, we expect primitiveCount to be a multiple of 2
		return primitiveCount * (usingPointSprites() ? 1 : 2);
	}

	bool CircleBatch::usingPointSprites() const
	{
		return mVertexOptions == VertexOptions::Points;
	}

	float CircleBatch::getRadius() const
	{
		return mRadius;
	}

}