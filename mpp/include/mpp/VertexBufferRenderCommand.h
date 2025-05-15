#pragma once

#include <cstdint>

#include "mpp/Config.h"

namespace mpp
{

	struct VertexBufferRenderCommand
	{
		uint32_t offset;
		uint32_t count{ ~0u };
	};

}