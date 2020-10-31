#include <iostream>
#include <cassert>

#include "mpp/UniformBuffer.h"
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
	UniformBuffer::UniformBuffer(RenderSystem* renderSystem, shared_ptr<const int8> data, size_t dataSize, uint32 binding)
		: mUBO(0)
		, mwRenderSystem(renderSystem)
		, mDataSize(dataSize)
		, mBinding(binding)
	{
		mData.reserve(dataSize);
		int8 const* dataPtr = data.get();

		for (size_t i = 0; i < dataSize; ++i)
		{
			mData.push_back(*dataPtr++);
		}
	}

	/*
	 * Destructor.
	 *
	 */
	UniformBuffer::~UniformBuffer()
	{
		unload();
	}

	/*
	 * Return vertex data for potential modification
	 *
	 */
	vector<int8>& UniformBuffer::getBufferData()
	{
		return mData;
	}

	/*
	 * Allocate buffer memory.
	 *
	 */
	void UniformBuffer::allocate()
	{
		GLenum glStorageType = GL_DYNAMIC_DRAW;
		GL_CHECK(glBufferData(GL_UNIFORM_BUFFER, mDataSize, &(mData[0]), glStorageType));
	}

	/*
	 * Reupload the buffer data.  This method pulls the whole buffer memory down, and back up, even
	 * if only a part of it has been updated.  Thus it may be inefficient if only a small part of the
	 * buffer has been modified.
	 *
	 */
	void UniformBuffer::mapBufferData()
	{
		bind();

		int8* bufferPtr{ nullptr };
		GL_CHECK(bufferPtr = (int8*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));

		memcpy(bufferPtr, &(mData[0]), mDataSize);
		GL_CHECK(glUnmapBuffer(GL_ARRAY_BUFFER));
	}

	/*
	 * Bind buffer
	 *
	 */
	void UniformBuffer::bind()
	{
		GL_CHECK(glBindBuffer(GL_UNIFORM_BUFFER, mUBO));
	}

	/*
	 * Unbind buffer
	 *
	 */
	void UniformBuffer::unbind()
	{
		GL_CHECK(glBindBuffer(GL_UNIFORM_BUFFER, 0));
	}

	/*
	 * Create GL buffer.
	 *
	 */
	void UniformBuffer::load()
	{
		unload();
		GL_CHECK(glGenBuffers(1, &mUBO));

		bind();
		allocate();

		glBindBufferBase(GL_UNIFORM_BUFFER, mBinding, mUBO);
	}

	/*
	 * Destroy GL buffer
	 *
	 */
	void UniformBuffer::unload()
	{
		if (mUBO != 0)
		{
			GL_CHECK(glDeleteBuffers(1, &mUBO));
			mUBO = 0;
		}
	}
}