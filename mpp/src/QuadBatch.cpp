#include <cmath>

#include "utils/MemTracker.h"

#include "mpp/QuadBatch.h"
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
	QuadBatch::QuadBatch(string const& name,
		VertexOptions vertexOptions,
		mpp::mesh::Vertex::DataType positionType,
		mpp::mesh::Vertex::DataType texcoordType,
		mpp::mesh::Vertex::DataType colourType,
		bool rotate,
		bool sameSize,
		int maxDimX,
		int maxDimY,
		string const& texture,
		bool textureAtlas,
		size_t indexWidth,
		size_t initialCapacity,
		string const& defaultVertexShader,
		string const& defaultFragmentShader,
		string const& descriptor,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Batch(name, initialCapacity, defaultVertexShader, defaultFragmentShader, "quad", renderSystem, resourceMgr)
		, mVertexOptions(vertexOptions)
		, mPositionType(positionType)
		, mTexcoordType(texcoordType)
		, mColourType(colourType)
		, mRotate(rotate)
		, mSameSize(sameSize)
		, mMaxDimX(maxDimX)
		, mMaxDimY(maxDimY)
		, mTexture(texture)
		, mTextureAtlas(textureAtlas)
		, mIndexWidth(indexWidth)
		, mPointSize((float)maxDimX)
	{
		// Set vertex options
		float size = (float)max(maxDimX, maxDimY);
		bool square = mSameSize && mMaxDimX == mMaxDimY;

		Caps const& caps = getRenderSystem()->getCaps();
		bool canUsePointSprites = square &&
			caps.pointSizeRange[0] < size && caps.pointSizeRange[1] > size;

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
	}

	/*
	 * Constructor.
	 *
	 */
	QuadBatch::QuadBatch(string const& name,
		VertexOptions vertexOptions,
		mpp::mesh::Vertex::DataType positionType,
		mpp::mesh::Vertex::DataType texcoordType,
		mpp::mesh::Vertex::DataType colourType,
		bool rotate,
		bool sameSize,
		int maxDimX,
		int maxDimY,
		string const& texture,
		bool textureAtlas,
		size_t indexWidth,
		size_t initialCapacity,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Batch(name, initialCapacity, VertexShader2dTemplate, FragmentShader2dTemplate, "quad", renderSystem, resourceMgr)
		, mVertexOptions(vertexOptions)
		, mPositionType(positionType)
		, mTexcoordType(texcoordType)
		, mColourType(colourType)
		, mRotate(rotate)
		, mSameSize(sameSize)
		, mMaxDimX(maxDimX)
		, mMaxDimY(maxDimY)
		, mTexture(texture)
		, mTextureAtlas(textureAtlas)
		, mIndexWidth(indexWidth)
		, mPointSize((float)maxDimX)
	{
		// Set vertex options
		float size = (float)max(maxDimX, maxDimY);
		bool square = mSameSize && mMaxDimX == mMaxDimY;

		Caps const& caps = getRenderSystem()->getCaps();
		bool canUsePointSprites = square &&
			caps.pointSizeRange[0] < size && caps.pointSizeRange[1] > size;


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

	}

	mesh::Primitive::Type QuadBatch::getPrimitiveType() const
	{
		return usingPointSprites() ? mesh::Primitive::Type::Points : mesh::Primitive::Type::Triangles;
	}

	bool QuadBatch::indexedVertices() const
	{
		return !usingPointSprites();
	}

	/*
	 * Create indices for a primitive.
	 *
	 */
	void QuadBatch::createIndexData(vector<uint8>& data, uint32_t start, size_t count)
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

	/*
	 * Get the number of vertices required, given the number of primitives.
	 *
	 */
	int QuadBatch::getVertexCount(int primitiveCount)
	{
		return primitiveCount * (usingPointSprites() ? 1 : 4);
	}

	void QuadBatch::createMeshSpecification(mesh::Primitive::Type primitiveType)
	{
		mSpecification = mesh::MeshSpecification(primitiveType);
		auto layout = mSpecification.createVertexBufferAttributeLayout();

		if (rotating())
		{
			layout->createAttribute(mesh::Vertex::Component::Position4, mPositionType, false);
		}
		else
		{
			layout->createAttribute(mesh::Vertex::Component::Position2, mPositionType, false);
		}

		if (usingPointSprites())
		{
			if (mTexture != "" && mTextureAtlas)
			{
				layout->createAttribute(mesh::Vertex::Component::TexCoord4, mTexcoordType, false);
			}
		}
		else if (rotating())
		{
			layout->createAttribute(mesh::Vertex::Component::TexCoord4, mTexcoordType, false);
		}
		else
		{
			layout->createAttribute(mesh::Vertex::Component::TexCoord2, mTexcoordType, false);
		}
		
		layout->createAttribute(mesh::Vertex::Component::Colour4, mColourType, true);
	}

	/*
	 * Create the data required.
	 *
	 */
	void QuadBatch::createImpl()
	{
		// Set primitive options
		auto primitiveType = getPrimitiveType();
		int primitiveCount = getPrimitiveCount(getCapacity());

		createMeshSpecification(primitiveType);

		// Set program flags
		uint32 flags = 0
			| (usingPointSprites() ? MPP_PROGRAM_TAGS_PRIM_POINTS : MPP_PROGRAM_TAGS_PRIM_TRIANGLES)
			| (mTexture != "" ? MPP_PROGRAM_TAGS_TEXTURE1 : 0)
			| (mTextureAtlas ? MPP_PROGRAM_TAGS_ATLAS : 0)
			| (rotating() ? MPP_PROGRAM_TAGS_ROTATION : 0);

		auto materialResource = createMaterial(getName() + "_QuadBatch", mTexture, flags);
		int vertexCount = getVertexCount(primitiveCount);

		vector<uint8> indices;
		createIndexData(indices, 0, getCapacity());

		auto mesh = new Mesh(
			getRenderSystem(),
			getName(),
			materialResource,
			primitiveType,
			primitiveCount,
			mIndexWidth,
			indices,
			mesh::VertexBufferStorageType::Dynamic,
			mPointSize);

		auto bufferSize = mSpecification.getVertexBufferAttributeLayout(0).getVertexSize();
		int8* data = new int8[vertexCount * bufferSize];
		shared_ptr<const int8> dataPtr(data, [](int8*p) { delete[] p; });

		createMesh(mesh, vertexCount, bufferSize, dataPtr);
	}

	void QuadBatch::finishUpdate(int count, bool updateTexCoords)
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

	int QuadBatch::getPrimitiveCount(int objectCount) const
	{
		return objectCount * (usingPointSprites() ? 1 : 2);
	}

	bool QuadBatch::usingPointSprites() const
	{
		return mVertexOptions == VertexOptions::Points;
	}

	bool QuadBatch::rotating() const
	{
		return mRotate;
	}

	bool QuadBatch::usingTexture() const
	{
		return mTexture != "";
	}

	bool QuadBatch::usingTextureAtlas() const
	{
		return mTextureAtlas;
	}

}