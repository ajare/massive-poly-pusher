#pragma once

#include <deque>
#include <functional>
#include <map>
#include <memory>

#include "mpp/Config.h"
#include "mpp/RenderGraph.h"
#include "mpp/RenderGraphTargets.h"
#include "mpp/RenderGraphFrameContext.h"

namespace mpp
{
	class RenderSystem;

	struct _MPPAPI GraphPassExecutionStats
	{
		GraphPassHandle pass;
		std::string name;
		double cpuMilliseconds{ 0.0 };
		double gpuMilliseconds{ 0.0 };
		bool gpuTimingAvailable{ false };
		bool gpuTimingSupported{ false };
		uint64_t primitivesSubmitted{ 0 };
		uint64_t trianglesSubmitted{ 0 };
		uint64_t fullscreenQuads{ 0 };
	};
	class RenderGraphPassFactoryRegistry;
	class RenderGraphScenePass;
	class RenderGraphTemplate;

	class _MPPAPI RenderGraphExecutionContext
	{
		RenderGraphTargets const* mTargets;
		UniformCollection const* mParameters;
		RenderGraphFrameContext const* mFrame;
		GraphPassInfo const* mPass;

	public:
		explicit RenderGraphExecutionContext(RenderGraphTargets const* targets, UniformCollection const* parameters = nullptr, RenderGraphFrameContext const* frame = nullptr, GraphPassInfo const* pass = nullptr);
		RenderTargetPtr getImage(GraphImageHandle image) const;
		UniformCollection const& getParameters() const;
		RenderGraphFrameContext const& getFrame() const;
		GraphPassInfo const& getPass() const;
	};

	// Executes graphics passes supplied by application callbacks. It creates a
	// temporary framebuffer view for each pass; graph textures remain owned by
	// RenderGraphTargets.
	class _MPPAPI RenderGraphExecutor
	{
		RenderSystem* mRenderSystem;
		RenderGraphPassFactoryRegistry const* mFactoryRegistry{ nullptr };
		RenderGraphTemplate const* mExecutingTemplate{ nullptr };
		RenderGraphFrameContext const* mFrameContext{ nullptr };
		std::map<uint32_t, std::function<void(RenderGraphExecutionContext const&)>> mCallbacks;
		std::map<uint32_t, std::unique_ptr<RenderGraphScenePass>> mScenePasses;
		std::map<uint32_t, UniformCollection> mParameterOverrides;
		std::vector<GraphPassExecutionStats> mLastExecutionStats;
		struct GpuTimingQuery { GraphPassHandle pass; std::string name; uint32_t begin{ 0 }, end{ 0 }; };
		struct GpuTimingResult { std::string name; double milliseconds{ 0.0 }; };
		std::deque<std::vector<GpuTimingQuery>> mPendingGpuTimings;
		std::map<uint32_t, GpuTimingResult> mGpuTimings;
		bool mGpuTimingSupported{ false };
		void collectGpuTimings();
		void clearGpuTimings();

	public:
		explicit RenderGraphExecutor(RenderSystem* renderSystem);
		~RenderGraphExecutor();
		RenderGraphExecutor(RenderGraphExecutor const&) = delete;
		RenderGraphExecutor& operator =(RenderGraphExecutor const&) = delete;

		void setPassCallback(GraphPassHandle pass, std::function<void(RenderGraphExecutionContext const&)> callback);
		void setPassFactoryRegistry(RenderGraphPassFactoryRegistry const* registry);
		void setFrameContext(RenderGraphFrameContext const* frameContext);
		void setPassParameterOverrides(GraphPassHandle pass, UniformCollection const& parameters);
		void clearPassCallbacks();
		std::vector<GraphPassExecutionStats> const& getLastExecutionStats() const;
		void execute(RenderGraph const& graph, RenderGraphTargets const& targets, Caps const& caps);
		void execute(RenderGraphTemplate const& graphTemplate, RenderGraphTargets const& targets, Caps const& caps);
	};
}
