#include <iostream>
#include <cassert>

#include "mpp/VertexBuffer.h"
#include "mpp/RenderSystem.h"
#include "mpp/MppException.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{
	using namespace mpp::mesh;

	/*
	 * Constructor.  Pass already-created vertex data directly in.
	 *
	 */
	VertexBuffer::VertexBuffer(RenderSystem* renderSystem, VertexBufferStorageType storageType, int vertexCount, int vertexStride, bool streaming, bool staticData, shared_ptr<const int8> data)
		: mVBO(0)
		, mwRenderSystem(renderSystem)
		, mStorageType(storageType)
		, mVertexCount(vertexCount)
		, mVertexStride(vertexStride)
		, mStreaming(streaming)
		, mStatic(staticData)
	{
		mData.reserve(vertexCount * vertexStride);
		int8 const* dataPtr = data.get();

		for (int i = 0; i < vertexCount * vertexStride; ++i)
		{
			mData.push_back(*dataPtr++);
		}

		mMaxDataSize = vertexCount * vertexStride;
	}

	/*
	 * Destructor.
	 *
	 */
	VertexBuffer::~VertexBuffer()
	{
		unload();
	}

	/*
	 * Set an attribute for a program to use.
	 *
	 */
	void VertexBuffer::setAttribute(int id, Vertex::DataType dataType, int componentSize, int offset, bool normalise)
	{
		Attribute attr;
		
		attr.id = id;

		switch (dataType)
		{
		case Vertex::DataType::Byte:							attr.dataType = GL_BYTE; break;
		case Vertex::DataType::UnsignedByte:					attr.dataType = GL_UNSIGNED_BYTE; break;
		case Vertex::DataType::Short:							attr.dataType = GL_SHORT; break;
		case Vertex::DataType::UnsignedShort:					attr.dataType = GL_UNSIGNED_SHORT; break;
		case Vertex::DataType::Int:								attr.dataType = GL_INT; break;
		case Vertex::DataType::UnsignedInt:						attr.dataType = GL_UNSIGNED_INT; break;
		case Vertex::DataType::HalfFloat:						attr.dataType = GL_HALF_FLOAT; break;
		case Vertex::DataType::Float:							attr.dataType = GL_FLOAT; break;
		case Vertex::DataType::Double:							attr.dataType = GL_DOUBLE; break;
		case Vertex::DataType::Int_2_10_10_10_REV:				attr.dataType = GL_INT_2_10_10_10_REV; break;
		case Vertex::DataType::UnsignedInt_2_10_10_10_REV:		attr.dataType = GL_UNSIGNED_INT_2_10_10_10_REV; break;
		default:								
			THROW_MPP("Unsupported datatype.", __LINE__, __FILE__, __func__);
		}

		attr.componentSize = componentSize;
		attr.sizeInBytes = componentSize * Vertex::getDataTypeSize(dataType);
		attr.offsetInBytes = offset;
		attr.normalise = normalise;

		mAttributes.push_back(attr);
	}

	/*
	 * Get number of vertices in buffer.
	 *
	 */
	int VertexBuffer::getNumVertices() const
	{
		return mVertexCount;
	}

	/*
	 * Get vertex stride size.
	 *
	 */
	int VertexBuffer::getVertexStride() const
	{
		return mVertexStride;
	}

	/*
	 * Is the data static?
	 *
	 */
	bool VertexBuffer::isStatic() const
	{
		return mStatic;
	}

	/*
	 * Get the number of attributes this buffer has.
	 *
	 */
	int VertexBuffer::getNumAttributes() const
	{
		return (int)mAttributes.size();
	}

	/*
	 * Enable specified attribute.
	 *
	 */
	void VertexBuffer::enableAttribute(int index, bool enable)
	{
		assert((index >= 0 && index < getNumAttributes()) && "VertexBuffer::enableAttribute() 'index' argument out of range!");

		Attribute const& attrib = mAttributes[index];

		if (enable)
		{
			GL_CHECK(glEnableVertexAttribArray(attrib.id));

			switch (attrib.dataType)
			{
			case GL_BYTE:
			case GL_UNSIGNED_BYTE:
			case GL_SHORT:
			case GL_UNSIGNED_SHORT:
			case GL_INT:
			case GL_UNSIGNED_INT:
			case GL_INT_2_10_10_10_REV:
			case GL_UNSIGNED_INT_2_10_10_10_REV:
				if (attrib.normalise)
				{
					GL_CHECK(glVertexAttribPointer(attrib.id, attrib.componentSize, attrib.dataType, attrib.normalise ? GL_TRUE : GL_FALSE, mVertexStride, (const GLvoid*)(attrib.offsetInBytes)));
				}
				else
				{
					GL_CHECK(glVertexAttribIPointer(attrib.id, attrib.componentSize, attrib.dataType, mVertexStride, (const GLvoid*)(attrib.offsetInBytes)));
				}
				break;

			case GL_FLOAT:
			case GL_HALF_FLOAT:
				GL_CHECK(glVertexAttribPointer(attrib.id, attrib.componentSize, attrib.dataType, attrib.normalise ? GL_TRUE : GL_FALSE, mVertexStride, (const GLvoid*)(attrib.offsetInBytes)));
				break;

			case GL_DOUBLE:
				GL_CHECK(glVertexAttribLPointer(attrib.id, attrib.componentSize, attrib.dataType, mVertexStride, (const GLvoid*)(attrib.offsetInBytes)));
				break;

			default:
				THROW_MPP("Unsupported GL data in databuffer.", __LINE__, __FILE__, __func__);
			}
		}
		else
		{
			GL_CHECK(glDisableVertexAttribArray(attrib.id));
		}
	}

	/*
	 * Return vertex data for potential modification
	 *
	 */
	vector<int8>& VertexBuffer::getBufferData()
	{
		return mData;
	}

	/*
	 * Allocate buffer memory.
	 *
	 */
	void VertexBuffer::allocate(size_t size)
	{
		GLenum glStorageType;
		switch (mStorageType)
		{
		case mesh::VertexBufferStorageType::Static:
			glStorageType = GL_STATIC_DRAW;
			break;

		case mesh::VertexBufferStorageType::Dynamic:
			glStorageType = GL_DYNAMIC_DRAW;
			break;
		}

		if (mStreaming)
		{
			THROW_MPP_NOTIMP("geometry streaming", __LINE__, __FILE__, __func__);

			//GLbitfield fMap = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
			//GLbitfield fCreate = fMap | GL_DYNAMIC_STORAGE_BIT;

			if (size == 0)
			{
				//glBufferStorage(GL_ARRAY_BUFFER, size, nullptr, fCreate);
			}
			else
			{
				//glBufferStorage(GL_ARRAY_BUFFER, size, &(mData[0]), fCreate);
			}
		}
		else
		{
			if (size == 0)
			{
				GL_CHECK(glBufferData(GL_ARRAY_BUFFER, size, nullptr, glStorageType));
			}
			else
			{
				GL_CHECK(glBufferData(GL_ARRAY_BUFFER, size, &(mData[0]), glStorageType));
			}
		}

		mMaxDataSize = size;
	}

	/*
	 * Reupload the buffer data.  This method pulls the whole buffer memory down, and back up, even
	 * if only a part of it has been updated.  Thus it may be inefficient if only a small part of the
	 * buffer has been modified.
	 *
	 */
	void VertexBuffer::mapBufferData(int numVertices)
	{
		bind();

		// If the data has increased in size, then reallocate
		size_t curSize = numVertices * mVertexStride;
		if (curSize > mMaxDataSize)
		{
			allocate(curSize);
		}
		
		int8* bufferPtr{ nullptr };
		GL_CHECK(bufferPtr = (int8*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));

		memcpy(bufferPtr, &(mData[0]), curSize);
		GL_CHECK(glUnmapBuffer(GL_ARRAY_BUFFER));
	}

	/*
	 * Reupload the buffer data.  This method only updates the specified section of the buffer, so may be more
	 * efficient for only modifying a small part.
	 *
	 */
	void VertexBuffer::mapBufferData(int startVertex, int numVertices)
	{
		bind();

		// If (startVertex + numVertices) is greater than max vertices, then reallocate
		size_t curSize = (startVertex + numVertices) * mVertexStride;
		if (curSize > mMaxDataSize)
		{
			allocate(curSize);
		}
		
		GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, startVertex * mVertexStride, numVertices * mVertexStride, &(mData[startVertex * mVertexStride])));
	}

	/*
	 * Bind buffer
	 *
	 */
	void VertexBuffer::bind()
	{
		GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, mVBO));
	}

	/*
	 * Unbind buffer
	 *
	 */
	void VertexBuffer::unbind()
	{
		GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
	}

	/*
	 * Create GL buffer.
	 *
	 */
	void VertexBuffer::load()
	{
		unload();
		GL_CHECK(glGenBuffers(1, &mVBO));
	
		bind();
		allocate(mVertexStride * mVertexCount);
			
		for (int i = 0; i < getNumAttributes(); ++i)
		{
			enableAttribute(i, true);
		}
	}

	/*
	 * Destroy GL buffer
	 *
	 */
	void VertexBuffer::unload()
	{
		if (mVBO != 0)
		{
			GL_CHECK(glDeleteBuffers(1, &mVBO));
			mVBO = 0;
		}
	}
}