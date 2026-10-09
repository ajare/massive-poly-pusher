#pragma once

#include <string>

#include "mpp/Config.h"

namespace mpp
{
	enum class PbrShader
	{
		BuiltInVertex,
		BuiltInFragment
	};

	// Sources are loaded from the engine's shader resource directory during
	// RenderSystem initialisation.
	_MPPAPI std::string const& pbrShaderSource(PbrShader shader);
}
