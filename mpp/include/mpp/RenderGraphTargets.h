#pragma once

#include <map>
#include <vector>

#include "mpp/Config.h"
#include "mpp/RenderGraph.h"
#include "mpp/RenderTarget.h"
#include "mpp/RenderTextureStream.h"

namespace mpp
{
	class RenderSystem;
	_MPPAPI RenderTextureOptions makeGraphRenderTextureOptions(GraphImageDesc const& desc);
	class RenderGraphImportRegistry;

	// Owns the first, deliberately non-aliased set of physical render targets
	// for a graph allocation plan. Imported targets remain application-owned.
	class _MPPAPI RenderGraphTargets
	{
		struct PoolEntry
		{
			GraphImageLifetime lifetime;
			RenderTargetPtr target;
			RenderTargetPtr writeTarget;
		};

		RenderSystem* mRenderSystem;
		std::map<uint64_t, RenderTargetPtr> mTargets;
		std::map<uint64_t, RenderTargetPtr> mWriteTargets;
		std::map<uint32_t, RenderTargetPtr> mImportedTargets;
		std::vector<PoolEntry> mPool;

		static uint64_t makeKey(GraphImageHandle image);

	public:
		explicit RenderGraphTargets(RenderSystem* renderSystem);

		// Reuses compatible targets retained from previous allocate() calls.
		void allocate(RenderGraphAllocationPlan const& plan);
		// An import identifies backing storage, so every version of an external
		// logical image resolves to this target.
		void bindImported(GraphImageHandle image, RenderTargetPtr target);
		void bindImports(RenderGraph const& graph, RenderGraphImportRegistry const& imports);
		void clear();
		RenderTargetPtr get(GraphImageHandle image) const;
		RenderTargetPtr getWriteTarget(GraphImageHandle image) const;
		void resolve(GraphImageHandle image, bool depth) const;
	};
}
