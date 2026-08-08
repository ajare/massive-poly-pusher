#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "mpp/AntiAliasing.h"
#include "mpp/Config.h"
#include "mpp/RenderGraph.h"
#include "mpp/RenderPipelineOutput.h"
#include "mpp/RenderTarget.h"

namespace mpp
{
	class RenderSystem;

	// Renderer-owned storage requirements. These descriptors are compiled from
	// named outputs and are deliberately not part of graph/XML authoring.
	enum class PhysicalOutputImageRole { Input, Work, TaaColourHistory, TaaDepthHistory };

	struct _MPPAPI PhysicalOutputImageDesc
	{
		PhysicalOutputImageRole role{ PhysicalOutputImageRole::Input };
		glm::uvec2 size{ 0 };
		GraphImageFormat format{ GraphImageFormat::Rgba8 };
		uint32_t samples{ 1 };
	};

	struct _MPPAPI RenderPipelineOutputPlan
	{
		std::string name;
		std::string image;
		std::string taaDepth;
		AntiAliasingDefaults antiAliasing;
		glm::uvec2 logicalSize{ 0 };
		uint32_t rasterSamples{ 1 };
		std::vector<PhysicalOutputImageDesc> physicalImages;
	};

	// Owns one immutable generation of output-chain storage. Rebuild first
	// creates a complete candidate generation and swaps it in only on success.
	class _MPPAPI RenderOutputProcessor
	{
		struct OutputState
		{
			RenderPipelineOutputPlan plan;
			RenderTargetPtr input;
			std::vector<RenderTargetPtr> work;
			std::vector<RenderTargetPtr> colourHistory;
			RenderTargetPtr depthHistory;
		};

		RenderSystem* mRenderSystem;
		std::string mPipelineName;
		uint64_t mGeneration{ 0 };
		std::map<std::string, OutputState> mOutputs;
		std::vector<RenderPipelineOutputPlan> mPlans;

	public:
		RenderOutputProcessor(RenderSystem* renderSystem, std::string pipelineName);

		void rebuild(std::vector<RenderPipelineOutput> const& outputs, RenderGraph const& graph,
			std::map<std::string, RenderTargetPtr> const& destinations, AntiAliasingDefaults const& defaults);
		void clear();

		RenderTargetPtr getInput(std::string const& outputName) const;
		void present(std::string const& outputName, RenderTargetPtr const& destination, RenderTargetPtr const& source = {}) const;
		uint64_t getGeneration() const;
		std::vector<RenderPipelineOutputPlan> const& getPlans() const;
	};
}
