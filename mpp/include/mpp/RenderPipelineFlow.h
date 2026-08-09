#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "mpp/Config.h"
#include "mpp/RenderGraph.h"
#include "mpp/mesh/Primitive.h"

namespace mpp
{
	class SceneModel3d;

	enum class RenderFlowEventKind : uint8_t
	{
		PassBegin,
		PassEnd,
		BatchSubmission,
		MsaaResolve,
		Taa,
		SsaaHorizontal,
		SsaaVertical,
		Fxaa,
		Presentation
	};

	_MPPAPI char const* renderFlowEventKindName(RenderFlowEventKind kind);

	// A renderer submission descriptor. Batch population begins in process-flow
	// phase 2; defining the immutable snapshot contract here keeps later telemetry
	// additions out of the PipelineEditor UI model.
	struct _MPPAPI RenderBatchSubmission
	{
		uint64_t sequence{ 0 };
		GraphPassHandle parentPass;
		SceneModel3d const* sceneObject{ nullptr };
		std::string meshName;
		std::string materialName;
		std::string programName;
		std::vector<std::string> textureNames;
		mesh::Primitive::Type primitiveType{ mesh::Primitive::Type::Triangles };
		uint32_t offset{ 0 };
		uint32_t count{ 0 };
		size_t instanceCount{ 0 };
		bool transparent{ false };
		bool blend{ false };
		bool cullBackFaces{ false };
		bool wireframe{ false };
	};

	struct _MPPAPI RenderFlowEvent
	{
		RenderFlowEventKind kind{ RenderFlowEventKind::PassBegin };
		uint64_t sequence{ 0 };
		GraphPassHandle pass;
		GraphImageHandle image;
		std::string name;
	};

	// RenderPipeline publishes snapshots through shared_ptr<const ...>. The
	// renderer constructs a complete mutable candidate and publishes it only
	// after the frame succeeds, so consumers never observe partial execution.
	struct _MPPAPI RenderPipelineFlowSnapshot
	{
		uint64_t frameSerial{ 0 };
		uint64_t pipelineGeneration{ 0 };
		std::vector<GraphPassHandle> actualPassOrder;
		std::vector<RenderBatchSubmission> batches;
		std::vector<RenderFlowEvent> physicalEvents;
	};

	using RenderPipelineFlowSnapshotPtr = std::shared_ptr<RenderPipelineFlowSnapshot const>;
}
