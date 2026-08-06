#pragma once

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