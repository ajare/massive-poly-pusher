#pragma once

#include "Config.h"

#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#include <Windows.h>
#endif

#include <glew/glew.h>
#include <gl/gl.h>

#include <memory>
#include <vector>

namespace mpp
{
	class RenderSystem;

	class _MPPAPI UniformBuffer
	{
		GLuint mUBO;

		RenderSystem* mwRenderSystem;

		std::vector<int8> mData;

		size_t mDataSize;

	private:

		void allocate();

	public:

		UniformBuffer(RenderSystem* renderSystem, std::shared_ptr<const int8> data, size_t dataSize);

		virtual ~UniformBuffer();

		std::vector<int8>& getBufferData();

		void mapBufferData();

		void bind();

		void unbind();

		void load();

		void unload();
	};

}