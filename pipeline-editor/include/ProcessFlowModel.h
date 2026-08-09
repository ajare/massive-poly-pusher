#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>

#include "mpp/RenderGraph.h"
#include "mpp/RenderPipelineFlow.h"

namespace pipeline_editor
{
	enum class ProcessFlowNodeKind
	{
		AuthoredPass,
		BatchSubmission,
		BatchGroup,
		MsaaResolve,
		Taa,
		Ssaa,
		Fxaa,
		Presentation,
		AuthoredImage,
		Import,
		NamedOutput,
		TaaHistory,
		PhysicalWorkTarget
	};

	enum class ProcessFlowEdgeKind { Execution, Colour, Depth, Shadow, History, Import, Output };
	enum class ProcessFlowResourceCategory : uint32_t
	{
		None = 0,
		AuthoredImages = 1u << 0,
		Imports = 1u << 1,
		NamedOutputs = 1u << 2,
		MsaaResources = 1u << 3,
		TaaHistories = 1u << 4,
		SsaaTargets = 1u << 5,
		FxaaTargets = 1u << 6
	};

	struct ProcessFlowFilters
	{
		uint32_t resources{ 0 };
		bool executionEdges{ true };
		bool resourceEdges{ true };
		bool visible(ProcessFlowResourceCategory category) const;
	};

	struct ProcessFlowNode
	{
		uint64_t id{ 0 };
		std::string semanticKey;
		std::string title;
		std::string subtitle;
		std::string details;
		ProcessFlowNodeKind kind{ ProcessFlowNodeKind::AuthoredPass };
		ProcessFlowResourceCategory resourceCategory{ ProcessFlowResourceCategory::None };
		uint64_t sequence{ 0 };
		size_t submissionCount{ 0 };
		int authoredPosition{ -1 };
		int actualPosition{ -1 };
		int passId{ -1 };
		int imageId{ -1 };
		int importIndex{ -1 };
		std::string materialName;
		std::vector<int> sceneObjectIndices;
		std::vector<std::string> sceneObjectNames;
		bool enabled{ true };
		bool mainSpine{ false };
		bool orderWarning{ false };
		bool expanded{ false };
		std::string bypassReason;
		glm::vec2 position{ 0.0f };
		glm::vec2 size{ 220.0f, 76.0f };
		float layoutRank{ 0.0f };
	};

	struct ProcessFlowEdge
	{
		uint64_t id{ 0 };
		uint64_t source{ 0 };
		uint64_t destination{ 0 };
		ProcessFlowEdgeKind kind{ ProcessFlowEdgeKind::Execution };
		std::string label;
	};

	struct ProcessFlowModel
	{
		uint64_t pipelineGeneration{ 0 };
		uint64_t frameSerial{ 0 };
		uint64_t sceneGeneration{ 0 };
		uint64_t revision{ 0 };
		std::vector<ProcessFlowNode> nodes;
		std::vector<ProcessFlowEdge> edges;
		std::vector<std::string> diagnostics;
		std::string emptyState;
		std::string warningBanner;
		bool stale{ false };
		bool liveSample{ false };
		bool largeGraph{ false };

		ProcessFlowNode* findNode(uint64_t id);
		ProcessFlowNode const* findNode(uint64_t id) const;
	};

	struct ProcessFlowSceneObjectRef
	{
		int index{ -1 };
		std::string name;
	};

	struct ProcessFlowBuildInput
	{
		mpp::RenderGraph const* graph{ nullptr };
		mpp::RenderPipelineFlowSnapshotPtr snapshot;
		std::vector<mpp::RenderPipelineOutputPlan> outputPlans;
		std::vector<std::string> passBypassReasons;
		std::unordered_map<std::string, int> imports;
		std::unordered_map<void const*, ProcessFlowSceneObjectRef> sceneObjects;
		uint64_t sceneGeneration{ 0 };
		bool stale{ false };
		std::string staleReason;
		ProcessFlowFilters filters;
	};

	class ProcessFlowSampleGate
	{
		double mLastPoll{ -1.0 };
		double mInterval{ 0.25 };

	public:
		explicit ProcessFlowSampleGate(double intervalSeconds = 0.25) : mInterval(intervalSeconds) {}
		bool poll(double timestampSeconds, bool force = false);
		void reset() { mLastPoll = -1.0; }
	};

	class ProcessFlowModelBuilder
	{
		uint64_t mRevision{ 0 };

	public:
		ProcessFlowModel build(ProcessFlowBuildInput const& input);
	};

	void runProcessFlowModelTests();
}
