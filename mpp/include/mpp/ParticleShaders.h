#pragma once

#include <string>

#include "mpp/Config.h"

namespace mpp
{
	enum class ParticleShader
	{
		PoolInitialiseCompute,
		StatisticsPrepareCompute,
		EventPrepareCompute,
		SpawnCompute,
		SimulationPrepareCompute,
		SimulationCompute,
		EventProcessCompute,
		CompactionPrepareCompute,
		CompactionCountCompute,
		CompactionPrefixCompute,
		CompactionScatterCompute,
		MeshCommandCompute,
		SortPrepareCompute,
		SortKeyCompute,
		RadixHistogramCompute,
		RadixPrefixCompute,
		RadixScatterCompute,
		SortFinalizeCompute,
		DrawVertex,
		DrawFragment,
		VolumetricLightingVertex,
		VolumetricLightingFragment,
		DistortionCompositeFragment,
		WeightedOitResolveFragment
	};

	// Sources are loaded from the engine's shader resource directory during
	// RenderSystem initialisation.
	_MPPAPI std::string const& particleShaderSource(ParticleShader shader);
}
