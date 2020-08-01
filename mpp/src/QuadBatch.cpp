#include <cmath>

#include "utils/MemTracker.h"

#include "mpp/QuadBatch.h"
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
		ColourOptions colourOptions,
		RotationOptions rotationOptions,
		bool sameSize,
		int maxDimX,
		int maxDimY,
		ResourcePtr program,
		ResourcePtr texture,
		bool textureAtlas,
		int indexWidth,
		uint32 initialCount,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Batch(name, colourOptions, false, initialCount, renderSystem, resourceMgr)
		, mRotate(false)
		, mVertexOptions(vertexOptions)
		, mTexCoordOptions(TexCoordsOptions::None)
		, mPositionType(positionType)
		, mTexcoordType(texcoordType)
		, mRotationOptions(rotationOptions)
		, mSameSize(sameSize)
		, mMaxDimX(maxDimX)
		, mMaxDimY(maxDimY)
		, mUsePointSprites(false)
		, mProgram(program)
		, mTexture(texture)
		, mTextureAtlas(textureAtlas)
		, mTexCoordBufferStride(0)
		, mIndexWidth(indexWidth)
		, mPointSize((float)maxDimX)
	{
	}

	/*
	 * Write a default vertex.
	 *
	 */
	void QuadBatch::writeVertex(float x, float y, float u, float v, bool rotate, int8** mainPtr, int8** texCoordPtr)
	{
		// Position
		writeFloat(x, mainPtr);
		writeFloat(y, mainPtr);

		// Rotation
		if (rotate)
		{
			writeFloat(1, mainPtr);
			writeFloat(0, mainPtr);
		}

		// Tex coords
		if (mTexCoordOptions == TexCoordsOptions::TexCoords2)
		{
			writeFloat(u, texCoordPtr);
			writeFloat(v, texCoordPtr);
		}

		// Colour
		switch (mColourOptions)
		{
		case ColourOptions::FloatRGBA:
			writeFloat(1, mainPtr);
		case ColourOptions::FloatRGB:
			writeFloat(1, mainPtr);
			writeFloat(1, mainPtr);
		case ColourOptions::FloatAlpha:
			writeFloat(1, mainPtr);
			break;
		case ColourOptions::UByteRGBA:
			writeUByte(255, mainPtr);
		case ColourOptions::UByteRGB:
			writeUByte(255, mainPtr);
			writeUByte(255, mainPtr);
		case ColourOptions::UByteAlpha:
			writeUByte(255, mainPtr);
			break;
		default:
			break;
		}
	}

	/*
	 * Write a default vertex.
	 *
	 */
	void QuadBatch::writeVertex(float x, float y, float u0, float v0, float u1, float v1, bool rotate, int8** mainPtr, int8** texCoordPtr)
	{
		// Position
		writeFloat(x, mainPtr);
		writeFloat(y, mainPtr);

		// Rotation
		if (rotate)
		{
			writeFloat(1, mainPtr);
			writeFloat(0, mainPtr);
		}

		// Tex coords
		if (mTexCoordOptions == TexCoordsOptions::TexCoords4)
		{
			writeFloat(u0, texCoordPtr);
			writeFloat(v0, texCoordPtr);
			writeFloat(u1, texCoordPtr);
			writeFloat(v1, texCoordPtr);
		}

		// Colour
		switch (mColourOptions)
		{
		case ColourOptions::FloatRGBA:
			writeFloat(1, mainPtr);
		case ColourOptions::FloatRGB:
			writeFloat(1, mainPtr);
			writeFloat(1, mainPtr);
		case ColourOptions::FloatAlpha:
			writeFloat(1, mainPtr);
			break;
		case ColourOptions::UByteRGBA:
			writeUByte(255, mainPtr);
		case ColourOptions::UByteRGB:
			writeUByte(255, mainPtr);
			writeUByte(255, mainPtr);
		case ColourOptions::UByteAlpha:
			writeUByte(255, mainPtr);
			break;
		default:
			break;
		}
	}

	/*
	 * Set stride for main (non-texture) buffer
	 *
	 */
	void QuadBatch::setMainBufferStride()
	{
		mMainBufferStride = Vertex::getDataTypeSize(mPositionType) * 2; // X,Y

		if (mRotate && mUsePointSprites)
		{
			mMainBufferStride += Vertex::getDataTypeSize(mPositionType) * 2; // Rotation data
		}

		mColourOffset = mMainBufferStride;

		switch (mColourOptions)
		{
		case ColourOptions::FloatAlpha:
		case ColourOptions::UByteRGBA:
			mMainBufferStride += 4;
			break;
		case ColourOptions::FloatRGB:
			mMainBufferStride += 12;
			break;
		case ColourOptions::FloatRGBA:
			mMainBufferStride += 16;
			break;
		case ColourOptions::UByteAlpha:
			mMainBufferStride += 1;
			break;
		case ColourOptions::UByteRGB:
			mMainBufferStride += 3;
			break;
		}
	}

	/*
	 * Set stride for texture buffer
	 *
	 */
	void QuadBatch::setTexCoordBufferStride()
	{
		if (mTexCoordOptions == TexCoordsOptions::TexCoords2)
		{
			mTexCoordBufferStride = Vertex::getDataTypeSize(mTexcoordType) * 2; // U,V
		}
		else if (mTexCoordOptions == TexCoordsOptions::TexCoords4)
		{
			mTexCoordBufferStride = Vertex::getDataTypeSize(mTexcoordType) * 4; // U0,V0,U1,V1
		}
	}

	/*
	 * Set the data pointers for the mesh specification.
	 *
	 */
	void QuadBatch::setSpecificationPointers(VertexBuffer* mainBuffer, VertexBuffer* texCoordBuffer)
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
						//attrib.data = nullptr;
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
	 * Create indices for a primitive.
	 *
	 */
	int QuadBatch::setIndices(uint32* ptr, uint32 base)
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

	/*
	 * Get the number of vertices required, given the number of primitives.
	 *
	 */
	int QuadBatch::getVertexCount(int primitiveCount)
	{
		return primitiveCount * (mUsePointSprites ? 1 : 4);
	}

	/*
	 * Is this batch using texture coordinates?
	 *
	 */
	bool QuadBatch::useTexCoords() const
	{
		return mTexCoordOptions != TexCoordsOptions::None;
	}

	/*
	 * Create the data required.
	 *
	 */
	void QuadBatch::createImpl()
	{
		bool square = mSameSize && mMaxDimX == mMaxDimY;
		bool useColours = mColourOptions != ColourOptions::None;

		Caps const& caps = getRenderSystem()->getCaps();
		bool canUsePointSprites = square &&
			caps.pointSizeRange[0] < mMaxDimX && caps.pointSizeRange[1] > mMaxDimX;

		if (mVertexOptions == VertexOptions::Points && !canUsePointSprites)
		{
			THROW_MPP("Cannot use point sprites for this QuadBatch.", __LINE__, __FILE__, __func__);
		}

		mUsePointSprites = canUsePointSprites && mVertexOptions != VertexOptions::Triangles;

		if (mRotationOptions != RotationOptions::None && !mUsePointSprites)
		{
			THROW_MPP("Cannot rotate this QuadBatch unless using point sprites.", __LINE__, __FILE__, __func__);
		}

		mRotate = mRotationOptions != RotationOptions::None && mUsePointSprites;

		// We use texture coords if:
		// - We are using triangles (texcoords2)
		// - If we are using points:
		//   - If are using a texture atlas (texcoords4)
		//   - If we are rotating:
		//     - If we are not using an atlas (texcoords2)
		//     - If we are using an atlas (texcoords4)
		mTexCoordOptions = TexCoordsOptions::None;
		if (mTexture)
		{
			if (!mUsePointSprites)
			{
				mTexCoordOptions = TexCoordsOptions::TexCoords2;
			}
			else
			{
				if (mTextureAtlas)
				{
					mTexCoordOptions = TexCoordsOptions::TexCoords4;
				}
			}
		}

		auto primitiveType = mUsePointSprites ? mesh::Primitive::Type::Points : mesh::Primitive::Type::Triangles;
		auto storageType = mesh::VertexBufferStorageType::Dynamic;

		// Set program flags
		uint32 flags{ 0 };

		if (mTexture)
		{
			flags |= MPP_PROGRAM_TAGS_TEXTURE1;
		}

		if (mUsePointSprites)
		{
			flags |= MPP_PROGRAM_TAGS_PRIM_POINTS;
		}
		else
		{
			flags |= MPP_PROGRAM_TAGS_PRIM_TRIANGLES;
		}

		// Create material
		string materialName = getName() + "_QuadBatch";

		mSpecification = mesh::MeshSpecification(primitiveType);
		auto layout = mSpecification.createVertexBufferAttributeLayout();

		if (mUsePointSprites && mRotate)
		{
			flags |= MPP_PROGRAM_TAGS_ROTATION;
			if (mTextureAtlas)
			{
				flags |= MPP_PROGRAM_TAGS_ATLAS;
			}

			mPositionOptions = PositionOptions::Position4;
			layout->createAttribute(mesh::Vertex::Component::Position4, mPositionType, false);
		}
		else
		{
			mPositionOptions = PositionOptions::Position2;
			layout->createAttribute(mesh::Vertex::Component::Position2, mPositionType, false);
		}
#
		switch (mTexCoordOptions)
		{
		case TexCoordsOptions::TexCoords2:
			layout->createAttribute(mesh::Vertex::Component::TexCoord2, mTexcoordType, false);
			break;

		case TexCoordsOptions::TexCoords4:
			layout->createAttribute(mesh::Vertex::Component::TexCoord4, mTexcoordType, false);
			break;

		case TexCoordsOptions::None:
		default:
			break;
		}

		if (useColours)
		{
			mesh::Vertex::Component component;
			mesh::Vertex::DataType dataType;
			switch (mColourOptions)
			{
			case ColourOptions::FloatAlpha:
				component = mesh::Vertex::Component::Colour1;
				dataType = mesh::Vertex::DataType::Float;
				break;
			case ColourOptions::UByteAlpha:
				component = mesh::Vertex::Component::Colour1;
				dataType = mesh::Vertex::DataType::UnsignedByte;
				break;
			case ColourOptions::FloatRGB:
				component = mesh::Vertex::Component::Colour3;
				dataType = mesh::Vertex::DataType::Float;
				break;
			case ColourOptions::UByteRGB:
				component = mesh::Vertex::Component::Colour3;
				dataType = mesh::Vertex::DataType::UnsignedByte;
				break;
			case ColourOptions::FloatRGBA:
				component = mesh::Vertex::Component::Colour4;
				dataType = mesh::Vertex::DataType::Float;
				break;
			case ColourOptions::UByteRGBA:
				component = mesh::Vertex::Component::Colour4;
				dataType = mesh::Vertex::DataType::UnsignedByte;
				break;
			default:
				THROW_MPP("Unsupported colour options.", __LINE__, __FILE__, __func__);
			}

			layout->createAttribute(component, dataType, true);
		}

		// Create material
		auto materialResource = mProgram
			? createMaterial(getName() + "_QuadBatch", mProgram, mTexture ? mTexture->getName() : "__mpp_tex_none__", flags)
			: createMaterial(getName() + "_QuadBatch", mTexture ? mTexture->getName() : "__mpp_tex_none__", flags);

		// Set up vertex data.
		int primitiveCount = mUsePointSprites ? mMaxCount * 1 : mMaxCount * 2;
		int vertexCount = getVertexCount(primitiveCount);

		// Get stride
		setMainBufferStride();
		setTexCoordBufferStride();

		int8* mainData = nullptr;
		int8* texCoordData = nullptr;

		mainData = new int8[vertexCount * mMainBufferStride];
		if (useTexCoords())
		{
			texCoordData = new int8[vertexCount * mTexCoordBufferStride];
		}
		
		shared_ptr<const int8> mainDataPtr(mainData, [](int8*p) { delete[] p; });
		shared_ptr<const int8> texCoordDataPtr(nullptr, [](int8*p) { delete[] p; });;

		if (useTexCoords())
		{
			texCoordDataPtr.reset(texCoordData);
		}

		// Index data
		vector<uint8> indices;
		if (!mUsePointSprites)
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
		Mesh* mesh = mUsePointSprites
			? new Mesh(getRenderSystem(), getName(), materialResource, primitiveType, primitiveCount, storageType, (float)mMaxDimX)
			: new Mesh(getRenderSystem(), getName(), materialResource, primitiveType, primitiveCount, mIndexWidth, indices, storageType, (float)mMaxDimX);

		VertexBuffer* mainBuffer = mesh->createVertexBuffer(vertexCount, mMainBufferStride, false, mainDataPtr);
		VertexBuffer* texCoordBuffer = mTexCoordOptions != TexCoordsOptions::None ? mesh->createVertexBuffer(vertexCount, mTexCoordBufferStride, false, texCoordDataPtr) : nullptr;

		// Set specification buffers
		setSpecificationPointers(mainBuffer, texCoordBuffer);

		// Add attributes to buffer
		int mainBufferOffset = 0;
		int curAttrib = 0;

		if (mRotate)
		{
			mainBuffer->setAttribute(curAttrib, mPositionType, 4, mainBufferOffset, false); // Pos4
			mainBufferOffset += 4 * Vertex::getDataTypeSize(mPositionType);
		}
		else
		{
			mainBuffer->setAttribute(curAttrib, mPositionType, 2, mainBufferOffset, false); // Pos2
			mainBufferOffset += 2 * Vertex::getDataTypeSize(mPositionType);
		}

		curAttrib++;

		switch (mTexCoordOptions)
		{
		case TexCoordsOptions::TexCoords2:
			texCoordBuffer->setAttribute(curAttrib, mTexcoordType, 2, 0, false);
			curAttrib++;
			break;

		case TexCoordsOptions::TexCoords4:
			texCoordBuffer->setAttribute(curAttrib, mTexcoordType, 4, 0, false);
			curAttrib++;
			break;

		case TexCoordsOptions::None:
		default:
			break;
		}

		if (useColours)
		{
			switch (mColourOptions)
			{
			case ColourOptions::FloatAlpha:
				mainBuffer->setAttribute(curAttrib, mesh::Vertex::DataType::Float, 1, mainBufferOffset, false);
				mainBufferOffset += 4;
				break;
			case ColourOptions::FloatRGB:
				mainBuffer->setAttribute(curAttrib, mesh::Vertex::DataType::Float, 3, mainBufferOffset, false);
				mainBufferOffset += 12;
				break;
			case ColourOptions::FloatRGBA:
				mainBuffer->setAttribute(curAttrib, mesh::Vertex::DataType::Float, 4, mainBufferOffset, false);
				mainBufferOffset += 16;
				break;
			case ColourOptions::UByteAlpha:
				mainBuffer->setAttribute(curAttrib, mesh::Vertex::DataType::UnsignedByte, 1, mainBufferOffset, true);
				mainBufferOffset += 1;
				break;
			case ColourOptions::UByteRGB:
				mainBuffer->setAttribute(curAttrib, mesh::Vertex::DataType::UnsignedByte, 3, mainBufferOffset, true);
				mainBufferOffset += 3;
				break;
			case ColourOptions::UByteRGBA:
				mainBuffer->setAttribute(curAttrib, mesh::Vertex::DataType::UnsignedByte, 4, mainBufferOffset, true);
				mainBufferOffset += 4;
				break;
			default:
				break;
			}
			
			curAttrib++;
		}

		mMeshes.push_back(mesh);
	}
	
	void QuadBatch::setMinimumCount(int count)
	{
		if (count > mMaxCount)
		{
			mpp::VertexBuffer* vertexBuffer0 = mMeshes[0]->getVertexBuffer(0), *vertexBuffer1 = nullptr;
			auto& data = vertexBuffer0->getBufferData();

			int newSize = getVertexCount(count) * mMainBufferStride;
			data.resize(newSize);

			if (useTexCoords())
			{
				vertexBuffer1 = mMeshes[0]->getVertexBuffer(1);
				auto& data = vertexBuffer1->getBufferData();

				int newSize = count * getVertexCount(count) * mTexCoordBufferStride;
				data.resize(newSize);
			}

			// Index data
			if (!mUsePointSprites)
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
			setSpecificationPointers(vertexBuffer0, vertexBuffer1);
		}
	}

	void QuadBatch::finishUpdate(int count, bool updateTexCoords)
	{
		mCurCount = count;

		if (mMeshes[0]->isIndexed())
		{
			mMeshes[0]->mapIndexData(count * (mUsePointSprites ? 1 : 2));
		}

		mpp::VertexBuffer* vertexBuffer0 = mMeshes[0]->getVertexBuffer(0);
		vertexBuffer0->mapBufferData(getVertexCount(count));
		mMeshes[0]->setNumPrimitives(mUsePointSprites ? count : count * 2);

		if (updateTexCoords && useTexCoords())
		{
			mpp::VertexBuffer* vertexBuffer1 = mMeshes[0]->getVertexBuffer(1);
			vertexBuffer1->mapBufferData(getVertexCount(count));
		}
	}

	void QuadBatch::setPointSize(float size)
	{
		mPointSize = size;
		for (auto mesh: mMeshes)
		{
			mesh->setPointSize(size);
		}
	}

	float QuadBatch::getPointSize() const
	{
		return mPointSize;
	}

	size_t QuadBatch::getPositionStride() const
	{
		return mMainBufferStride;
	}

	size_t QuadBatch::getTexCoordStride() const
	{
		return mTexCoordBufferStride;
	}

	size_t QuadBatch::getColourStride() const
	{
		return mMainBufferStride;
	}

	bool QuadBatch::usePointSprites() const
	{
		return mUsePointSprites;
	}

	int QuadBatch::getPrimitiveCount() const
	{
		return getCount() * (mUsePointSprites ? 1 : 2);
	}

	bool QuadBatch::getRotate() const
	{
		return mRotate;
	}

	QuadBatch::TexCoordsOptions QuadBatch::getTexCoordOptions() const
	{
		return mTexCoordOptions;
	}

	QuadBatch::PositionOptions QuadBatch::getPositionOptions() const
	{
		return mPositionOptions;
	}

}