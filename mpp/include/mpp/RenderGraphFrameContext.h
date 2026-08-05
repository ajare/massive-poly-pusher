#pragma once

#include <vector>

#include "mpp/Camera.h"
#include "mpp/Scene.h"
#include "mpp/SceneModel3d.h"
#include "mpp/RenderPass.h"

namespace mpp
{
	class RenderSystem;
	struct RenderPipelineOptions;

	// Live application state supplied for one graph execution. XML references
	// pass factories only; it never serializes these runtime objects.
	struct _MPPAPI RenderGraphFrameContext
	{
		RenderSystem* renderSystem{ nullptr };
		ScenePtr scene;
		CameraPtr camera;
		std::vector<SceneModel3dPtr> visibleModels;
		RenderPipelineOptions const* pipelineOptions{ nullptr };
		RenderPassPtr sceneRenderPass;
	};
}
