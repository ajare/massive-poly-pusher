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
		mpp::mesh::Vertex::DataType positionType,
		ColourOptions colourOptions,
		bool useDiffuseColour,
		uint32 initialCount,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Batch(name, colourOptions, useDiffuseColour, initialCount, VertexShader2dTemplate, FragmentShader2dTemplate, "", renderSystem, resourceMgr)
		, mPositionType(positionType)
	{
	}

	/*
	 * Write a default vertex.
	 *
	 */
	void LineBatch::writeVertex(float x, float y, int8** mainPtr)
	{
		// Position
		writeFloat(x, mainPtr);
		writeFloat(y, mainPtr);
		
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
	void LineBatch::setMainBufferStride()
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
	void LineBatch::setTexCoordBufferStride()
	{
	}

	/*
	 * Set the data pointers for the mesh specification.
	 *
	 */
	void LineBatch::setSpecificationPointers(VertexBuffer* mainBuffer, VertexBuffer* texCoordBuffer)
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
	 * Get the number of vertices required, given the number of primitives.
	 *
	 */
	int LineBatch::getVertexCount(int primitiveCount)
	{
		return primitiveCount * 2;
	}

	/*
	 * Create the data required.
	 *
	 */
	void LineBatch::createImpl()
	{
		bool useColours = mColourOptions != ColourOptions::None;

		auto primitiveType = mesh::Primitive::Type::Lines;
		auto storageType = mesh::VertexBufferStorageType::Dynamic;

		uint32 flags = 0;

		mSpecification = mesh::MeshSpecification(primitiveType);
		auto layout = mSpecification.createVertexBufferAttributeLayout();

		layout->createAttribute(mesh::Vertex::Component::Position2, mPositionType, false);

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
				THROW_MPP("Unknown colour options.", __LINE__, __FILE__, __func__);
			}

			layout->createAttribute(colourComponent, colourType, normaliseColours);
		}

		// Allow diffuse colouring
		if (mUseDiffuse)
		{
			flags |= MPP_PROGRAM_TAGS_DIFFUSE;
		}

		// Create material
		auto materialResource = createMaterial(getName() + "_LineBatch", "__mpp_tex_none__", flags);

		// Set up vertex data.
		int primitiveCount = mMaxCount;
		int vertexCount = getVertexCount(primitiveCount);

		// Get stride
		setMainBufferStride();
		setTexCoordBufferStride();

		int8* mainData = nullptr;

		mainData = new int8[vertexCount * mMainBufferStride];

		shared_ptr<const int8> mainDataPtr(mainData, [](int8*p) { delete[] p; });
		
		for (int i = 0; i < vertexCount; ++i)
		{
			writeVertex(0.0f, 0.0f, &mainData);
		}

		// IMPROVE: 
		// optional hint to give a hard maximum count, so if using triangles we can use
		// 16-bit indices if allowed.
		Mesh* mesh = new Mesh(getRenderSystem(), getName(), materialResource, primitiveType, primitiveCount, storageType);
		VertexBuffer* mainBuffer = mesh->createVertexBuffer(vertexCount, mMainBufferStride, false, mainDataPtr);

		// Set specification buffers
		setSpecificationPointers(mainBuffer, nullptr);

		// Add attributes to buffer
		int mainBufferOffset = 0, curAttrib = 0;

		mainBuffer->setAttribute(curAttrib, mPositionType, 2, 0, false);
		mainBufferOffset += Vertex::getDataTypeSize(mPositionType) * 2;
		curAttrib++;

		if (useColours)
		{
			mainBuffer->setAttribute(curAttrib, colourType, Vertex::getComponentSize(colourComponent), mainBufferOffset, normaliseColours);
			curAttrib++;
		}

		mMeshes.push_back(mesh);
	}
	
	void LineBatch::setMinimumCount(int count)
	{
		if (count > mMaxCount)
		{
			mpp::VertexBuffer* vertexBuffer0 = mMeshes[0]->getVertexBuffer(0), *vertexBuffer1 = nullptr;
			auto& data = vertexBuffer0->getBufferData();

			int newSize = getVertexCount(count) * mMainBufferStride;
			data.resize(newSize);
			
			mMaxCount = count;
			setSpecificationPointers(vertexBuffer0, nullptr);
		}
	}

	void LineBatch::finishUpdate(int count, bool updateTexCoords)
	{
		mCurCount = count;

		// If streaming then just set primitive count
		mpp::VertexBuffer* vertexBuffer0 = mMeshes[0]->getVertexBuffer(0);
		vertexBuffer0->mapBufferData(getVertexCount(count));
		mMeshes[0]->setNumPrimitives(count);
	}

}