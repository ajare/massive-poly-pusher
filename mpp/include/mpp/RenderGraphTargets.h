#pragma once

#include <map>

#include "mpp/Config.h"
#include "mpp/RenderGraph.h"
#include "mpp/RenderTarget.h"

namespace mpp
{
	class RenderSystem;

	// Owns the first, deliberately non-aliased set of physical render targets
	// for a graph allocation plan. Imported targets remain application-owned.
	class _MPPAPI RenderGraphTargets
	{
		RenderSystem* mRenderSystem;
		std::map<uint64_t, RenderTargetPtr> mTargets;
		std::map<uint32_t, RenderTargetPtr> mImportedTargets;

		static uint64_t makeKey(GraphImageHandle image);

	public:
		explicit RenderGraphTargets(RenderSystem* renderSystem);

		void allocate(RenderGraphAllocationPlan const& plan);
		// An import identifies backing storage, so every version of an external
		// logical image resolves to this target.
		void bindImported(GraphImageHandle image, RenderTargetPtr target);
		void clear();
		RenderTargetPtr get(GraphImageHandle image) const;
	};
}
