#pragma once

#include <vector>
#include <memory>

#include "mpp/Config.h"
#include "mpp/Resource.h"
#include "mpp/SceneModel3d.h"
#include "mpp/Camera.h"
#include "mpp/RenderTarget.h"
#include "mpp/Colour.h"

namespace mpp
{
	class RenderSystem;

	class _MPPAPI RenderPass
	{
		RenderSystem* mRenderSystem;

		RenderTargetPtr mTarget;

	public:

		explicit RenderPass(RenderSystem* renderSystem);

		virtual ~RenderPass();

		RenderTargetPtr getRenderTarget();

		void bindRenderTarget();

		virtual void render(std::vector<SceneModel3dPtr> const& models, CameraPtr camera);
	};

	typedef std::shared_ptr<RenderPass> RenderPassPtr;
}