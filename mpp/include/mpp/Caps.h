#pragma once

#include <cstdint>

namespace mpp
{

	struct Caps
	{
		int glVersionMajor{ 0 };
		int glVersionMinor{ 0 };
		int glslVersionMajor{ 0 };
		int glslVersionMinor{ 0 };

		float pointSizeRange[2]{ 0.0f, 0.0f };
		float aliasedLineWidthRange[2]{ 0.0f, 0.0f };

		int maxTextureSize{ 0 };
		int maxRectTextureSize{ 0 };

		// Framebuffer limits used by graph/MRT validation and execution.
		uint32_t maxColourAttachments{ 1 };
		uint32_t maxDrawBuffers{ 1 };
		uint32_t maxSamples{ 1 };
		// Bit N is set when N-sample MSAA is supported for standard RGBA8
		// render targets. Output validation/allocation uses the 2, 4 and 8 bits.
		uint32_t supportedMsaaSampleMask{ 0 };
		bool supportsMsaa(uint32_t samples) const
		{
			return samples == 1 || (samples < 32 && (supportedMsaaSampleMask & (1u << samples)) != 0);
		}

		float depthRange[2]{ 0.0f, 0.0f };

		int maxRecommendedElements{ 0 };
		int maxRecommendedVertices{ 0 };
		int maxElements{ 0 };

		// One means anisotropic filtering is unavailable or has no effect.
		float maxAnisotropy{ 1.0f };

		bool streamingGeometry{ false };

		uint32_t maxVertexShaderUniforms{ 0 };
		uint32_t maxGeometryShaderUniforms{ 0 };
		uint32_t maxFragmentShaderUniforms{ 0 };

		uint32_t maxVertexTextureUnits{ 0 };
		uint32_t maxGeometryTextureUnits{ 0 };
		uint32_t maxFragmentTextureUnits{ 0 };

		uint32_t maxVertexAttributes{ 0 };
		uint32_t maxVertexAttributeStride{ 0 };
	};

}