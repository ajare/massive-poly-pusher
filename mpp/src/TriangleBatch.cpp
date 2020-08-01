#include <cmath>

#include "utils/MemTracker.h"

#include "mpp/TriangleBatch.h"
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
		mpp::mesh::Vertex::DataType positionType,
		mpp::mesh::Vertex::DataType texcoordType,
		ColourOptions colourOptions,
		bool useDiffuseColour,
		ResourcePtr program,
		ResourcePtr texture,
		uint32 initialCount,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Batch(name, colourOptions, useDiffuseColour, initialCount, renderSystem, resourceMgr)
		, mPositionType(positionType)
		, mTexcoordType(texcoordType)
		, mProgram(program)
		, mTexture(texture)
		, mUseTexCoords(texture != nullptr)
		, mTexCoordBufferStride(0)
	{
	}

	/*
	 * Write a default vertex.
	 *
	 */
	void TriangleBatch::writeVertex(float x, float y, float u, float v, int8** mainPtr, int8** texCoordPtr)
	{
		// Position
		writeFloat(x, mainPtr);
		writeFloat(y, mainPtr);

		// Tex coords
		if (mUseTexCoords)
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
	 * Set stride for main (non-texture) buffer
	 *
	 */
	void TriangleBatch::setMainBufferStride()
	{
		mMainBufferStride = Vertex::getDataTypeSize(mPositionType) * 2; // X,Y

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
	void TriangleBatch::setTexCoordBufferStride()
	{
		if (mUseTexCoords)
		{
			mTexCoordBufferStride = Vertex::getDataTypeSize(mTexcoordType) * 2; // U,V
		}
	}

	/*
	 * Set the data pointers for the mesh specification.
	 *
	 */
	void TriangleBatch::setSpecificationPointers(VertexBuffer* mainBuffer, VertexBuffer* texCoordBuffer)
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
					{
						mPositionData = (char*)&((mainBuffer->getBufferData()[0]));
					}
					else
					{
						mPositionData = nullptr;
					}
					break;

				case Vertex::Component::TexCoord2:
				case Vertex::Component::TexCoord4:
					if (texCoordBuffer->getBufferData().size() > 0)
					{
						mTexCoordData = (char*)&((texCoordBuffer->getBufferData()[0]));
					}
					else
					{
						mTexCoordData = nullptr;
					}
					break;

				case Vertex::Component::Colour1:
				case Vertex::Component::Colour3:
				case Vertex::Component::Colour4:
					if (mainBuffer->getBufferData().size() > 0)
					{
						mColourData = (char*)&((mainBuffer->getBufferData()[0])) + mColourOffset;
					}
					else
					{
						mColourData = nullptr;
					}
					break;
				}
			}
		}
	}

	/*
	 * Get the number of vertices required, given the number of primitives.
	 *
	 */
	int TriangleBatch::getVertexCount(int primitiveCount)
	{
		return primitiveCount * 3;
	}

	/*
	 * Create the data required.
	 *
	 */
	void TriangleBatch::createImpl()
	{
		bool useColours = mColourOptions != ColourOptions::None;

		auto primitiveType = mesh::Primitive::Type::Triangles;
		auto storageType = mesh::VertexBufferStorageType::Dynamic;

		uint32 flags = 0;

		mSpecification = mesh::MeshSpecification(primitiveType);
		auto layout = mSpecification.createVertexBufferAttributeLayout();

		layout->createAttribute(mesh::Vertex::Component::Position2, mPositionType, false);

		if (mUseTexCoords)
		{
			layout->createAttribute(mesh::Vertex::Component::TexCoord2, mTexcoordType, false);
		}

		mesh::Vertex::Component colourComponent = mesh::Vertex::Component::Unused;
		mesh::Vertex::DataType colourType = mesh::Vertex::DataType::None;
		bool normaliseColours = false;
		if (useColours)
		{
			switch (mColourOptions)
			{
			case ColourOptions::FloatAlpha:
				colourComponent = mesh::Vertex::Component::Colour1;
				colourType = mesh::Vertex::DataType::Float;
				break;
			case ColourOptions::UByteAlpha:
				colourComponent = mesh::Vertex::Component::Colour1;
				colourType = mesh::Vertex::DataType::UnsignedByte;
				normaliseColours = true;
				break;
			case ColourOptions::FloatRGB:
				colourComponent = mesh::Vertex::Component::Colour3;
				colourType = mesh::Vertex::DataType::Float;
				break;
			case ColourOptions::UByteRGB:
				colourComponent = mesh::Vertex::Component::Colour3;
				colourType = mesh::Vertex::DataType::UnsignedByte;
				normaliseColours = true;
				break;
			case ColourOptions::FloatRGBA:
				colourComponent = mesh::Vertex::Component::Colour4;
				colourType = mesh::Vertex::DataType::Float;
				break;
			case ColourOptions::UByteRGBA:
				colourComponent = mesh::Vertex::Component::Colour4;
				colourType = mesh::Vertex::DataType::UnsignedByte;
				normaliseColours = true;
				break;
			default:
				THROW_MPP("Unsupported colour options.", __LINE__, __FILE__, __func__);
			}

			layout->createAttribute(colourComponent, colourType, normaliseColours);
		}

		// Allow diffuse colouring
		if (mUseDiffuse)
		{
			flags |= MPP_PROGRAM_TAGS_DIFFUSE;
		}

		// Create material
		auto materialResource = mProgram
			? createMaterial(getName() + "_TriangleBatch", mProgram, mTexture ? mTexture->getName() : "__mpp_tex_none__", flags)
			: createMaterial(getName() + "_TriangleBatch", mTexture ? mTexture->getName() : "__mpp_tex_none__", flags);

		// Set up vertex data.
		int primitiveCount = mMaxCount;
		int vertexCount = getVertexCount(primitiveCount);

		// Get stride
		setMainBufferStride();
		setTexCoordBufferStride();

		int8* mainData = nullptr;
		int8* texCoordData = nullptr;

		mainData = new int8[vertexCount * mMainBufferStride];
		if (mUseTexCoords)
		{
			texCoordData = new int8[vertexCount * mTexCoordBufferStride];
		}

		shared_ptr<const int8> mainDataPtr(mainData, [](int8*p) { delete[] p; });
		shared_ptr<const int8> texCoordDataPtr(nullptr, [](int8*p) { delete[] p; });

		if (mUseTexCoords)
		{
			texCoordDataPtr.reset(texCoordData);
		}

		// Write default vertices
		for (int i = 0; i < vertexCount; ++i)
		{
			writeVertex(0.0f, 0.0f, 0.0f, 0.0f, &mainData, &texCoordData);
		}

		// IMPROVE: 
		// optional hint to give a hard maximum count, so if using triangles we can use
		// 16-bit indices if allowed.
		Mesh* mesh = new Mesh(getRenderSystem(), getName(), materialResource, primitiveType, primitiveCount, storageType);

		VertexBuffer* mainBuffer = mesh->createVertexBuffer(vertexCount, mMainBufferStride, false, mainDataPtr);
		VertexBuffer* texCoordBuffer = mUseTexCoords ? mesh->createVertexBuffer(vertexCount, mTexCoordBufferStride, false, texCoordDataPtr) : nullptr;

		// Set specification buffers
		setSpecificationPointers(mainBuffer, texCoordBuffer);

		// Add attributes to buffer
		int mainBufferOffset = 0, curAttrib = 0;

		mainBuffer->setAttribute(curAttrib, mPositionType, 2, 0, false);
		mainBufferOffset += Vertex::getDataTypeSize(mPositionType) * 2;
		curAttrib++;

		if (mUseTexCoords)
		{
			texCoordBuffer->setAttribute(curAttrib, mTexcoordType, 2, 0, false);
			curAttrib++;
		}

		if (useColours)
		{
			mainBuffer->setAttribute(curAttrib, colourType, Vertex::getComponentSize(colourComponent), mainBufferOffset, normaliseColours);
			curAttrib++;
		}

		mMeshes.push_back(mesh);

		postCreate();
	}

	void TriangleBatch::setMinimumCount(int count)
	{
		mpp::VertexBuffer* vertexBuffer0 = mMeshes[0]->getVertexBuffer(0);
		auto& data = vertexBuffer0->getBufferData();

		int currentVertices = data.size() / mMainBufferStride;
		int requiredVertices = getVertexCount(count);

		if (requiredVertices > currentVertices)
		{
			mpp::VertexBuffer* vertexBuffer1 = nullptr;

			data.resize(requiredVertices * mMainBufferStride);

			if (mUseTexCoords)
			{
				vertexBuffer1 = mMeshes[0]->getVertexBuffer(1);
				auto& data = vertexBuffer1->getBufferData();

				data.resize(requiredVertices * mTexCoordBufferStride);
			}

			mMaxCount = count;
			setSpecificationPointers(vertexBuffer0, vertexBuffer1);
		}
	}

	void TriangleBatch::finishUpdate(int count, bool updateTexCoords)
	{
		mCurCount = count;

		if (mMeshes[0]->isIndexed())
		{
			mMeshes[0]->mapIndexData(count);
		}

		mpp::VertexBuffer* vertexBuffer0 = mMeshes[0]->getVertexBuffer(0);
		vertexBuffer0->mapBufferData(getVertexCount(count));
		mMeshes[0]->setNumPrimitives(count);

		if (updateTexCoords && mUseTexCoords)
		{
			mpp::VertexBuffer* vertexBuffer1 = mMeshes[0]->getVertexBuffer(1);
			vertexBuffer1->mapBufferData(getVertexCount(count));
		}
	}

}