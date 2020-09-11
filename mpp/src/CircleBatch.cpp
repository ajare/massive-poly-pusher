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
		float maxRadius,
		float borderSize,
		int indexWidth,
		uint32 initialCount,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Batch2(name, initialCount, VertexShader2dCircle, FragmentShader2dCircle, "circle", renderSystem, resourceMgr)
		, mRadius(maxRadius)
		, mBorderSize(borderSize)
		, mPointSize(maxRadius * 2)
		, mVertexOptions(vertexOptions)
		, mPositionType(positionType)
		, mColourType(colourType)
		, mIndexWidth(indexWidth)
	{
	}

	/*
	 * Create indices for a primitive.
	 *
	 */
	int CircleBatch::setIndices(uint32* ptr, uint32 base)
	{
		int indexBytes = mIndexWidth / 8;
		if (indexBytes == 2)
		{
			*ptr = (base * 4 + 0) + ((base * 4 + 1) << 16); ptr++;
			*ptr = (base * 4 + 2) + ((base * 4 + 2) << 16); ptr++;
			*ptr = (base * 4 + 3) + ((base * 4 + 0) << 16); ptr++;
			return 3;
		}
		else if (indexBytes == 4)
		{
			*ptr = base * 4 + 0; ptr++;
			*ptr = base * 4 + 1; ptr++;
			*ptr = base * 4 + 2; ptr++;
			*ptr = base * 4 + 2; ptr++;
			*ptr = base * 4 + 3; ptr++;
			*ptr = base * 4 + 0; ptr++;
			return 6;
		}
		else
		{
			return 0;
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

		layout->createAttribute(mesh::Vertex::Component::UserDefined4, "SIZES", mesh::Vertex::DataType::Float, false);
		layout->createAttribute(mesh::Vertex::Component::UserDefined4, "BORDERCOLOUR", mColourType, true);
		layout->createAttribute(mesh::Vertex::Component::UserDefined4, "INNERCOLOUR", mColourType, true);
	}

	void CircleBatch::createMesh(Mesh* mesh, size_t vertexCount, size_t bufferSize, shared_ptr<const int8> dataPtr)
	{
		for (int i = 0; i < mSpecification.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto& layout = mSpecification.getVertexBufferAttributeLayout(i);
			auto vb = mesh->createVertexBuffer(vertexCount, bufferSize, false, dataPtr);

			for (int j = 0; j < layout.getNumAttributes(); ++j)
			{
				auto& attrib = layout.getAttribute(j);
				vb->setAttribute(
					attrib.attributeId,
					attrib.dataType,
					mesh::Vertex::getComponentSize(attrib.component),
					attrib.offsetInBytes,
					attrib.normalised);
			}
		}

		setSpecificationPointers(mesh);
		mMeshes.push_back(mesh);
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

		//
		// Create Mesh specification
		//
		auto primitiveType = usingPointSprites() ? mesh::Primitive::Type::Points : mesh::Primitive::Type::Triangles;
		auto storageType = mesh::VertexBufferStorageType::Dynamic;

		createMeshSpecification(primitiveType);

		//
		// Create material
		//
		uint32 flags = 0
			| (usingPointSprites() ? MPP_PROGRAM_TAGS_PRIM_POINTS : MPP_PROGRAM_TAGS_PRIM_TRIANGLES);

		string materialName = getName() + "_CircleBatch";

		auto materialResource = createMaterial(getName() + "_CircleBatch", "__mpp_tex_none__", flags);

		//
		// Create mesh
		//
		int primitiveCount = usingPointSprites() ? mMaxCount * 1 : mMaxCount * 2;
		int vertexCount = getVertexCount(primitiveCount);

		Mesh* mesh{ nullptr };
		if (usingPointSprites())
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
		else
		{
			// Index data
			vector<uint8> indices(mMaxCount * 6 * (mIndexWidth / 8));
			uint32* iPtr = (uint32*)&indices[0]; // Indices will be 16 or 32-bit, so use 32 to cover both

			for (int i = 0; i < mMaxCount; ++i)
			{
				iPtr += setIndices(iPtr, i);
			}

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

		for (size_t i = 0; i < mMeshes[0]->getNumVertexBuffers(); ++i)
		{
			auto vertexBuffer = mMeshes[0]->getVertexBuffer(i);
			vertexBuffer->mapBufferData(getVertexCount(count));
		}

		mMeshes[0]->setNumPrimitives(usingPointSprites() ? count : count * 2);
	}

	void CircleBatch::setPointSize(float size)
	{
		mPointSize = size;
		for (auto mesh : mMeshes)
		{
			mesh->setPointSize(size);
		}
	}

	float CircleBatch::getPointSize() const
	{
		return mPointSize;
	}

	int CircleBatch::getPrimitiveCount() const
	{
		return getCount() * (usingPointSprites() ? 1 : 2);
	}

	bool CircleBatch::usingPointSprites() const
	{
		return mVertexOptions == VertexOptions::Points;
	}

	/*
	 * Get the number of vertices required, given the number of primitives.
	 *
	 */
	int CircleBatch::getVertexCount(int primitiveCount)
	{
		return primitiveCount * (usingPointSprites() ? 1 : 4);
	}

	void CircleBatch::setMinimumCount(int count)
	{
		if (count > mMaxCount)
		{
			mpp::VertexBuffer* vertexBuffer0 = mMeshes[0]->getVertexBuffer(0), *vertexBuffer1 = nullptr;
			auto& data = vertexBuffer0->getBufferData();

			int newSize = getVertexCount(count) * vertexBuffer0->getVertexStride();
			data.resize(newSize);

			// Index data
			if (!usingPointSprites())
			{
				const int indexStride = 6 * (mIndexWidth / 8);
				newSize = count * indexStride;

				auto& indices = mMeshes[0]->getIndexData();
				indices.resize(newSize);

				uint32* iPtr = (uint32*)&(indices[mMaxCount * indexStride]);

				for (int i = mMaxCount; i < count; ++i)
				{
					iPtr += setIndices(iPtr, i);
				}
			}

			mMaxCount = count;
			setSpecificationPointers(mMeshes[0]);
		}
	}
}