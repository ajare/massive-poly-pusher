#pragma once

#include "mpp/Config.h"

namespace mpp
{
	class RenderGraphPassFactoryRegistry;

	// Registers reusable scene factories used by XML graph templates.
	_MPPAPI void registerBuiltInRenderGraphPasses(RenderGraphPassFactoryRegistry& registry);
}
