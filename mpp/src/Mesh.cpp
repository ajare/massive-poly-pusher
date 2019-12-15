#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#include <Windows.h>
#endif

#include <cassert>
#include <glew/glew.h>
#include <gl/gl.h>

#include "utils/MemTracker.h"

#include "mpp/Mesh.h"
#include "mpp/ModelStream.h"
#include "mpp/RenderSystem.h"

using namespace std;

namespace mpp
{
	/*
	 * Constructor (non-indexed).
	 *
	 */
	Mesh::Mesh(RenderSystem* renderSystem, string const& name, ResourcePtr material, mesh::Primitive::Type type, int primitiveCount, mesh::VertexBufferStorageType storageType, float pointSize)
		: mName(name)
		, mVAO(0)
		, mIBO(0)
		, mStorageType(storageType)
		, mIndexWidth(0)
		, mIndexDataSize(0)
		, mPointSize(pointSize)
		, mwRenderSystem(renderSystem)
		, mMaterial(material)
		, mIsIndexed(false)
		, mPrimitiveCount(primitiveCount)
		, mIsLoaded(false)
	{
		setPrimitiveData(type);
	}

	/*
	 * Constructor (indexed).
	 *
	 */
	Mesh::Mesh(RenderSystem* renderSystem, string const& name, ResourcePtr material, mesh::Primitive::Type type, int primitiveCount, int indexWidth, vector<uint8> const& indices, mesh::VertexBufferStorageType storageType, float pointSize)
		: mName(name)
		, mVAO(0)
		, mIBO(0)
		, mStorageType(storageType)
		, mIndexWidth(indexWidth)
		, mPointSize(pointSize)
		, mwRenderSystem(renderSystem)
		, mMaterial(material)
		, mIsIndexed(true)
		, mPrimitiveCount(primitiveCount)
	{
		setPrimitiveData(type);
		mIndexData = indices;
		//mIndexDataSize = (int)mIndexData.size();
	}

	/*
	 * Destructor.
	 *
	 */
	Mesh::~Mesh()
	{
		unload();

		for (auto vertexBuffer: mVertexBuffers)
		{
			delete vertexBuffer;
		}
	}

	/*
	 * Get name, to be used to manipulate instances.
	 *
	 */
	string const& Mesh::getName() const
	{
		return mName;
	}

	/*
	 * Set the primitive type/size
	 *
	 */
	void Mesh::setPrimitiveData(mesh::Primitive::Type type)
	{
		mPrimitiveType = type;
		switch (mPrimitiveType)
		{
		case mesh::Primitive::Type::Points:
			mPrimitiveRenderType = GL_POINTS;
			mPrimitiveSize = 1;
			break;

		case mesh::Primitive::Type::Lines:
			mPrimitiveRenderType = GL_LINES;
			mPrimitiveSize = 2;
			break;

		case mesh::Primitive::Type::Triangles:
			mPrimitiveRenderType = GL_TRIANGLES;
			mPrimitiveSize = 3;
			break;
		}
	}

	/*
	 * Set material.
	 *
	 */
	void Mesh::setMaterial(ResourcePtr material)
	{
		mMaterial = material;
	}

	/*
	 * Get material.
	 *
	 */
	ResourcePtr Mesh::getMaterial() const
	{
		return mMaterial;
	}

	/*
	 * Set index data.
	 *
	 */
	void Mesh::setIndexData(vector<uint8> const& indexData, int indexWidth)
	{
		mIsIndexed = true;
		mIndexData = indexData;
		mIndexDataSize = (int)mIndexData.size();
		mIndexWidth = indexWidth;
	}

	/*
	 * Are this mesh's vertices rendered indexed?
	 *
	 */
	bool Mesh::isIndexed() const
	{
		return mIsIndexed;
	}

	/*
	 * Set the number of primitives to render.
	 *
	 */
	void Mesh::setNumPrimitives(int count)
	{
		mPrimitiveCount = count;
	}

	/*
	 * Get primitive count.
	 *
	 */
	int Mesh::getNumPrimitives() const
	{
		return mPrimitiveCount;
	}

	/*
	 * Set point size.
	 *
	 */
	void Mesh::setPointSize(float size)
	{
		mPointSize = size;
	}

	/*
	 * Get point size.
	 *
	 */
	float Mesh::getPointSize() const
	{
		return mPointSize;
	}

	/*
	 * Add a vertex buffer.
	 *
	 */
	VertexBuffer* Mesh::createVertexBuffer(int vertexCount, int vertexStride, bool streaming, shared_ptr<const int8> vertexData)
	{
		VertexBuffer* buf = new VertexBuffer(mwRenderSystem, mStorageType, vertexCount, vertexStride, streaming, vertexData);

		mVertexBuffers.push_back(buf);
		return buf;
	}

	/*
	 * Get number of vertex buffers in this mesh.
	 *
	 */
	int Mesh::getNumVertexBuffers() const
	{
		return (int)mVertexBuffers.size();
	}

	/*
	 * Get indexed vertex buffer.
	 *
	 */
	VertexBuffer* Mesh::getVertexBuffer(int index)
	{
		assert((index >= 0 && index < getNumVertexBuffers()) && "Mesh::getVertexBuffer() 'index' argument out of range!");
		return mVertexBuffers[index];
	}

	/*
	 * Return index data for potential modification.
	 *
	 */
	vector<uint8>& Mesh::getIndexData()
	{
		return mIndexData;
	}

	/*
	 * Re-map index data.
	 *
	 */
	void Mesh::mapIndexData(int numPrimitives)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIBO);

		// If the data has increased in size, then reallocate
		int newSize = numPrimitives * mesh::Primitive::size(mPrimitiveType) * (mIndexWidth / 8);

		if (newSize > mIndexDataSize)
		{
			allocateIndexData(numPrimitives);
		}
		else
		{
			int8* bufferPtr = (int8*)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
			memcpy(bufferPtr, &(mIndexData[0]), mIndexDataSize);
			glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
		}
	}

	/*
	 * Re-map index data.
	 *
	 */
	void Mesh::mapIndexData(int startPrimitive, int numPrimitives)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIBO);

		// If the data has increased in size, then reallocate
		int indexStride = mesh::Primitive::size(mPrimitiveType) * (mIndexWidth / 8);
		int newSize = (startPrimitive + numPrimitives) * indexStride;

		if (newSize > mIndexDataSize)
		{
			allocateIndexData(startPrimitive + numPrimitives);
		}
		else
		{
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, startPrimitive * indexStride, numPrimitives * indexStride, &(mIndexData[startPrimitive * indexStride]));
		}
	}

	/*
	 * Allocate index data storage.
	 *
	 */
	void Mesh::allocateIndexData(int numPrimitives)
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

		mIndexDataSize = numPrimitives * mesh::Primitive::size(mPrimitiveType) * (mIndexWidth / 8);

		if (mIndexDataSize == 0)
		{
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, glStorageType);
		}
		else
		{
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIndexDataSize, &(mIndexData[0]), glStorageType);
		}
	}

	/*
	 * Load mesh into VRAM.
	 *
	 */
	void Mesh::load()
	{
		// Load into one vertex buffer
		glGenVertexArrays(1, &mVAO);
		glBindVertexArray(mVAO);

		// Create index buffer
		if (isIndexed())
		{
			glGenBuffers(1, &mIBO);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIBO);

			allocateIndexData(mPrimitiveCount);
		}

		// Create vertex buffers
		for (auto vertexBuffer: mVertexBuffers)
		{
			vertexBuffer->load();
		}

		// Release VAO
		glBindVertexArray(0);

		// Load material
		mMaterial->load();

		mIsLoaded = true;
	}

	/*
	* Unload mesh from VRAM.
	*
	*/
	void Mesh::unload()
	{
		if (mVAO != 0)
		{
			glDeleteVertexArrays(1, &mVAO);
			mVAO = 0;
		}
		if (mIBO != 0)
		{
			glDeleteBuffers(1, &mIBO);
			mIBO = 0;
		}

		for (auto vertexBuffer: mVertexBuffers)
		{
			vertexBuffer->unload();
		}

		mIsLoaded = false;
	}

	/*
	 * Bind the mesh for rendering.
	 *
	 */
	void Mesh::bind(bool use) const
	{
		if (use)
		{
			glBindVertexArray(mVAO);
			}
		else
		{
			glBindVertexArray(0);
		}
	}

	/*
	 * Send vertex data.
	 *
	 */
	void Mesh::render() const
	{
		render(mPrimitiveCount);
	}
		
	/*
	 * Send vertex data.
	 *
	 */
	void Mesh::render(uint32 numPrimitives) const
	{
		if (mPrimitiveType == mesh::Primitive::Type::Points)
		{
			glPointSize(mPointSize);
		}

		if (mIsIndexed)
		{
			GLenum indexType = mIndexWidth == 16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
			glDrawElements(mPrimitiveRenderType, numPrimitives * mPrimitiveSize, indexType, 0);
		}
		else
		{
			glDrawArrays(mPrimitiveRenderType, 0, numPrimitives * mPrimitiveSize);
		}
	}

	/*
	 * Set storage type.
	 *
	 */
	void Mesh::setStorageType(mesh::VertexBufferStorageType storageType)
	{
		mStorageType = storageType;

		if (mIsLoaded)
		{
			unload();
			load();
		}
	}

	/*
	 * Size (in bytes) of a vertex
	 *
	 */
	int Mesh::getVertexSize() const
	{
		int vertexSize = 0;
		for (auto const& vertexBuffer : mVertexBuffers)
		{
			vertexSize += vertexBuffer->getVertexStride();
		}

		return vertexSize;
	}
}