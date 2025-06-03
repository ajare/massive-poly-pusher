#pragma once

#include <cstdint>

#include "mpp/Config.h"
#include "mpp/Resource.h"

namespace mpp
{

	struct VertexBufferRenderCommand
	{
		uint32_t offset{ 0 };
		uint32_t count{ ~0u };
		ResourcePtr material{ nullptr };
		ResourcePtr textures[2] = { nullptr, nullptr };
		int clipMin[2] = { 0, 0 };
		int clipSize[2] = { -1, -1 };
	};

}