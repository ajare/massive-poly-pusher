#pragma once

#include <string>

#include "mpp/Config.h"
#include "mpp/UniformCollection.h"

namespace mpp
{
	class RenderGraphPassFactoryRegistry;

	// Registers reusable scene factories used by XML graph templates.
	_MPPAPI void registerBuiltInRenderGraphPasses(RenderGraphPassFactoryRegistry& registry);

	// Which blur level a bloom blur pass is, compared against the bloom blurPasses
	// option to decide whether it blurs or copies through. Prefers the authored
	// ITERATION parameter; falls back to trailing digits in the pass name for
	// graphs written before the parameter existed, which is why renaming such a
	// pass changes its behaviour. Exposed so the choice is testable without GL.
	_MPPAPI uint32_t bloomBlurIteration(UniformCollection const& parameters, std::string const& passName);
}
