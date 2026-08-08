#pragma once

#include <cstdint>

namespace mpp
{

	struct Caps
	{
		int glVersionMajor;
		int glVersionMinor;
		int glslVersionMajor;
		int glslVersionMinor;

		float pointSizeRange[2];
		float aliasedLineWidthRange[2];

		int maxTextureSize;
		int maxRectTextureSize;

		// Framebuffer limits used by graph/MRT validation and execution.
		uint32_t maxColourAttachments;
		uint32_t maxDrawBuffers;
		uint32_t maxSamples;
		// Bit N is set when N-sample MSAA is supported for standard RGBA8
		// render targets. Phase-one output validation uses the 2, 4 and 8 bits.
		uint32_t supportedMsaaSampleMask;
		bool supportsMsaa(uint32_t samples) const
		{
			return samples == 1 || (samples < 32 && (supportedMsaaSampleMask & (1u << samples)) != 0);
		}

		float depthRange[2];

		int maxRecommendedElements;
		int maxRecommendedVertices;
		int maxElements;

		float maxAnisotropy;

		bool streamingGeometry;

		uint32_t maxVertexShaderUniforms, maxGeometryShaderUniforms, maxFragmentShaderUniforms;

		uint32_t maxVertexTextureUnits, maxGeometryTextureUnits, maxFragmentTextureUnits;
	};

}