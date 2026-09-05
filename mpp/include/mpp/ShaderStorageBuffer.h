#pragma once

#include <cstdint>
#include <string>

#include "mpp/Config.h"

namespace mpp
{
	// A GPU-only shader storage buffer: allocated once, bound, never mapped, with
	// no CPU shadow copy and no triple buffering.
	//
	// detail::PersistentMappedBuffer covers the CPU-written case at
	// GL_SHADER_STORAGE_BUFFER, where segment rotation provides the
	// synchronisation. GPU-written buffers are synchronised with memory barriers
	// instead, and keeping the two types distinct is what keeps that difference
	// visible at the call site.
	class _MPPAPI ShaderStorageBuffer
	{
		uint32_t mBuffer{ 0 };

		size_t mSize{ 0 };

	public:

		ShaderStorageBuffer() = default;

		~ShaderStorageBuffer();

		ShaderStorageBuffer(ShaderStorageBuffer const&) = delete;

		ShaderStorageBuffer& operator =(ShaderStorageBuffer const&) = delete;

		// Allocates and zero-fills, or uploads initialData when supplied. The
		// contents are never read back: this is the GPU's memory.
		void create(size_t size, void const* initialData, std::string const& label);

		void destroy() noexcept;

		// Binds to a std430 binding point for compute and draw shaders.
		void bindStorage(uint32_t bindingIndex) const;

		// Binds as the source of glDraw*Indirect arguments.
		void bindDrawIndirect() const;

		// Binds as the source of glDispatchComputeIndirect arguments.
		void bindDispatchIndirect() const;

		uint32_t getBuffer() const { return mBuffer; }

		size_t getSize() const { return mSize; }
	};
}
