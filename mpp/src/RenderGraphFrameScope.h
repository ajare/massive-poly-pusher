#pragma once

#include "mpp/RenderGraphExecutor.h"

namespace mpp::detail
{
	// A pipeline's generated callbacks capture the current scene, camera and
	// models. Keep those captures within the frame, including failed frames.
	class RenderGraphFrameScope
	{
		RenderGraphExecutor& mExecutor;
		bool mClearCallbacks;

	public:
		RenderGraphFrameScope(RenderGraphExecutor& executor, bool clearCallbacks)
			: mExecutor(executor), mClearCallbacks(clearCallbacks)
		{
		}

		RenderGraphFrameScope(RenderGraphFrameScope const&) = delete;
		RenderGraphFrameScope& operator=(RenderGraphFrameScope const&) = delete;

		~RenderGraphFrameScope()
		{
			mExecutor.setFrameContext(nullptr);
			// clearPassCallbacks also clears persistent factory-created passes.
			// XML graphs retain those passes; generated graphs rebuild each frame.
			if (mClearCallbacks) mExecutor.clearPassCallbacks();
		}
	};
}
