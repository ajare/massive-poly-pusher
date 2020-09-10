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
		mpp::mesh::Vertex::DataType texcoordType,
		mpp::mesh::Vertex::DataType colourType,
		float maxRadius,
		float borderSize,
		int indexWidth,
		uint32 initialCount,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: QuadBatch(name, 
			vertexOptions,
			positionType,
			texcoordType,
			ColourOptions::FloatRGBA, 
			false, 
			false,
			(int)(maxRadius * 2),
			(int)(maxRadius * 2),
			nullptr,
			nullptr,
			false,
			indexWidth,
			initialCount, 
			VertexShader2dCircle,
			FragmentShader2dCircle,
			"circle",
			renderSystem, 
			resourceMgr)
		, mRadius(maxRadius)
		, mBorderSize(borderSize)
		, mColourType(colourType)
		, mNormalOffset(0)
		, mNormalData(nullptr)
	{
	}

	/*
	 * Set stride for main (non-texture) buffer
	 *
	 */
	void CircleBatch::setMainBufferStride()
	{
		// Position size
		if (usingPointSprites())
		{
			mMainBufferStride += Vertex::getDataTypeSize(mPositionType) * 2;
		}
		else
		{
			mMainBufferStride += Vertex::getDataTypeSize(mPositionType) * 4;
		}

		// Radius/border size
		mNormalOffset = mMainBufferStride;
		mMainBufferStride += Vertex::getDataTypeSize(mesh::Vertex::DataType::Float) * 4;

		// Colour size
		mColourOffset = mMainBufferStride;
		mMainBufferStride += Vertex::getDataTypeSize(mColourType) * 4;
	}

	/*
	 * Set stride for texture buffer
	 *
	 */
	void CircleBatch::setTexCoordBufferStride()
	{
		mTexCoordBufferStride = Vertex::getDataTypeSize(mTexcoordType) * 4;
	}

	/*
	 * Set the data pointers for the mesh specification.
	 *
	 */
	void CircleBatch::setSpecificationPointers(VertexBuffer* mainBuffer, VertexBuffer* texCoordBuffer)
	{
		for (int i = 0; i < mSpecification.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto& layout = mSpecification.getVertexBufferAttributeLayout(i);
			for (int j = 0; j < layout.getNumAttributes(); ++j)
			{
				auto& attrib = layout.getAttribute(j);

				switch (attrib.component)
				{
				case Vertex::Component::Position2:
				case Vertex::Component::Position4:
					if (mainBuffer->getBufferData().size() > 0)
						mPositionData = (char*)&((mainBuffer->getBufferData()[0]));
					else
						mPositionData = nullptr;
					break;

				case Vertex::Component::Normal4:
					if (mainBuffer->getBufferData().size() > 0)
						mNormalData = (char*)&((mainBuffer->getBufferData()[0])) + mNormalOffset;
					else
						mPositionData = nullptr;
					break;

				case Vertex::Component::TexCoord2:
				case Vertex::Component::TexCoord4:
					if (texCoordBuffer->getBufferData().size() > 0)
						mTexCoordData = (char*)&((texCoordBuffer->getBufferData()[0]));
					else
						mTexCoordData = nullptr;
					break;

				case Vertex::Component::Colour1:
				case Vertex::Component::Colour3:
				case Vertex::Component::Colour4:
					if (mainBuffer->getBufferData().size() > 0)
						mColourData = (char*)&((mainBuffer->getBufferData()[0])) + mColourOffset;
					else
						mColourData = nullptr;
					break;
				}
			}
		}
	}

	/*
	 * Create the data required.
	 *
	 */
	void CircleBatch::createImpl()
	{
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

		// Set position options: if we're using point sprites, we don't
		// need to store texcoords in the position
		if (usingPointSprites())
		{
			mPositionOptions = PositionOptions::Position2;
		}
		else
		{
			mPositionOptions = PositionOptions::Position4;
		}

		mTexCoordOptions = TexCoordsOptions::TexCoords4;

		// Set primitive options
		auto primitiveType = usingPointSprites() ? mesh::Primitive::Type::Points : mesh::Primitive::Type::Triangles;
		auto storageType = mesh::VertexBufferStorageType::Dynamic;

		// Set program flags
		uint32 flags = 0
			| (usingPointSprites() ? MPP_PROGRAM_TAGS_PRIM_POINTS : MPP_PROGRAM_TAGS_PRIM_TRIANGLES);

		// Create material
		string materialName = getName() + "_CircleBatch";

		mSpecification = mesh::MeshSpecification(primitiveType);
		auto layout = mSpecification.createVertexBufferAttributeLayout();

		if (mPositionOptions == PositionOptions::Position2)
		{
			layout->createAttribute(mesh::Vertex::Component::Position2, mPositionType, false);
		}
		else
		{
			layout->createAttribute(mesh::Vertex::Component::Position4, mPositionType, false);
		}

		// Radius/border size/etc is stored in normal
		layout->createAttribute(mesh::Vertex::Component::Normal4, mesh::Vertex::DataType::Float, false);

		// Border colour is stored in texcoords, main colour in colour
		layout->createAttribute(mesh::Vertex::Component::TexCoord4, mTexcoordType, true);
		layout->createAttribute(mesh::Vertex::Component::Colour4, mColourType, true);

		// Create material
		auto materialResource = createMaterial(getName() + "_CircleBatch", "__mpp_tex_none__", flags);

		// Set up vertex data.
		int primitiveCount = usingPointSprites() ? mMaxCount * 1 : mMaxCount * 2;
		int vertexCount = getVertexCount(primitiveCount);

		// Get stride
		setMainBufferStride();
		setTexCoordBufferStride();

		int8* mainData = new int8[vertexCount * mMainBufferStride];
		int8* texCoordData = new int8[vertexCount * mTexCoordBufferStride];

		shared_ptr<const int8> mainDataPtr(mainData, [](int8*p) { delete[] p; });
		shared_ptr<const int8> texCoordDataPtr(texCoordData, [](int8*p) { delete[] p; });;

		// Index data
		vector<uint8> indices;
		if (!usingPointSprites())
		{
			indices.resize(mMaxCount * 6 * (mIndexWidth / 8));
			uint32* iPtr = (uint32*)&indices[0]; // Indices will be 16 or 32-bit, so use 32 to cover both

			for (int i = 0; i < mMaxCount; ++i)
			{
				iPtr += setIndices(iPtr, i);
			}
		}

		// TODO: optional hint to give a hard maximum count, so if using triangles we can use 16-bit
		// indices if allowed.
		Mesh* mesh = usingPointSprites()
			? new Mesh(getRenderSystem(), getName(), materialResource, primitiveType, primitiveCount, storageType, size)
			: new Mesh(getRenderSystem(), getName(), materialResource, primitiveType, primitiveCount, mIndexWidth, indices, storageType, size);

		VertexBuffer* mainBuffer = mesh->createVertexBuffer(vertexCount, mMainBufferStride, false, mainDataPtr);
		VertexBuffer* texCoordBuffer = mTexCoordOptions != TexCoordsOptions::None ? mesh->createVertexBuffer(vertexCount, mTexCoordBufferStride, false, texCoordDataPtr) : nullptr;

		// Set specification buffers
		setSpecificationPointers(mainBuffer, texCoordBuffer);

		// Add attributes to buffer
		int mainBufferOffset = 0;

		// Position/texcoords
		if (usingPointSprites())
		{
			mainBuffer->setAttribute(0, mPositionType, 2, mainBufferOffset, false); // Pos4
			mainBufferOffset += 2 * Vertex::getDataTypeSize(mPositionType);
		}
		else
		{
			mainBuffer->setAttribute(0, mPositionType, 4, mainBufferOffset, false); // Pos4
			mainBufferOffset += 4 * Vertex::getDataTypeSize(mPositionType);
		}

		// Radius/border
		mainBuffer->setAttribute(1, mesh::Vertex::DataType::Float, 4, mainBufferOffset, false); // Normal4
		mainBufferOffset += 4 * Vertex::getDataTypeSize(mesh::Vertex::DataType::Float);

		// Border colour
		texCoordBuffer->setAttribute(2, mTexcoordType, 4, 0, true);

		// Inner colour
		mainBuffer->setAttribute(3, mColourType, 4, mainBufferOffset, true);
		mainBufferOffset += 4 * Vertex::getDataTypeSize(mColourType);

		mMeshes.push_back(mesh);
	}

	/*
	 * Get normal data.
	 *
	 */
	char* CircleBatch::getNormalData()
	{
		return mNormalData;
	}

	size_t CircleBatch::getNormalStride() const
	{
		return mMainBufferStride;
	}
}