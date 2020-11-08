#pragma once

#include <vector>
#include <memory>

#include "mpp/Config.h"
#include "mpp/RenderPass.h"
#include "mpp/PostEffect.h"
#include "mpp/Scene.h"

namespace mpp
{
	class RenderSystem;

	class _MPPAPI RenderPipeline
	{
		std::string mName;

		RenderSystem* mRenderSystem;

		std::vector<RenderPassPtr> mPasses;

		std::vector<ResourcePtr> mPostEffects;

	public:

		RenderPipeline(std::string const& name, RenderSystem* renderSystem);

		virtual ~RenderPipeline();

		std::string const& getName() const;

		RenderTargetPtr getOutputRenderTarget();

		void addRenderPass(RenderPassPtr pass);

		void addPostEffect(ResourcePtr effect);

		virtual void render(ScenePtr scene, CameraPtr camera);

	};

	typedef std::shared_ptr<RenderPipeline> RenderPipelinePtr;
}