#pragma once

#include "mpp/Config.h"

namespace mpp
{
	enum class AmbientOcclusionMethod
	{
		None,
		Ssao,
		Gtao
	};

	struct _MPPAPI SSAOOptions
	{
		float radius{ 0.5f };
		float intensity{ 1.0f };
		float bias{ 0.025f };
		float power{ 1.0f };
		int sampleCount{ 16 };
		int blurRadius{ 2 };
	};

	struct _MPPAPI GTAOOptions
	{
		float radius{ 1.0f };
		float intensity{ 1.0f };
		float thickness{ 0.5f };
		float horizonBias{ 0.05f };
		float falloffStart{ 0.1f };
		float falloffEnd{ 1.0f };
		int sliceCount{ 6 };
		int stepsPerSlice{ 4 };
		float power{ 1.0f };
		int blurRadius{ 2 };
	};

	struct _MPPAPI AmbientOcclusionOptions
	{
		AmbientOcclusionMethod method{ AmbientOcclusionMethod::None };
		SSAOOptions ssao;
		GTAOOptions gtao;
	};
}
