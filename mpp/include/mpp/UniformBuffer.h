#pragma once

#include "mpp/Config.h"

#include <memory>
#include <vector>

#include <glew/glew.h>
#include <gl/gl.h>

namespace mpp
{
	class RenderSystem;

	class _MPPAPI UniformBuffer
	{
		GLuint mUBO;

		RenderSystem* mwRenderSystem;

		std::vector<int8_t> mData;

		size_t mDataSize;
		
		uint32_t mBinding;

	private:

		void allocate();

	public:

		UniformBuffer(RenderSystem* renderSystem, std::shared_ptr<const int8_t> data, size_t dataSize, uint32_t binding);

		virtual ~UniformBuffer();

		std::vector<int8_t>& getBufferData();

		void updateData(uint32_t offset, size_t size);

		void mapBufferData();

		void bind();

		void activate();

		void unbind();

		void load();

		void unload();
	};

}