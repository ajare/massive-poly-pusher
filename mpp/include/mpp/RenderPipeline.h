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

	struct _MPPAPI PbrEnvironment
	{
		ResourcePtr irradianceMap;
		ResourcePtr prefilteredSpecularMap;
		ResourcePtr brdfIntegrationLut;
		ResourcePtr backgroundMap;
	};

	typedef std::shared_ptr<PbrEnvironment> PbrEnvironmentPtr;

	enum class RenderPipelineMode
	{
		LegacyForward,
		PbrForward
	};

	enum class PbrToneMapOperator
	{
		Reinhard,
		Aces
	};

	struct _MPPAPI RenderPipelineOptions
	{
		RenderPipelineMode mode{ RenderPipelineMode::LegacyForward };
		float exposure{ 1.0f };
		PbrToneMapOperator toneMapOperator{ PbrToneMapOperator::Aces };
		PbrEnvironmentPtr environment;
	};

	class _MPPAPI RenderPipeline
	{
		std::string mName;

		RenderSystem* mRenderSystem;

		RenderPipelineOptions mOptions;

		std::vector<RenderPassPtr> mPasses;

		std::vector<ResourcePtr> mPostEffects;

	public:

		RenderPipeline(std::string const& name, RenderSystem* renderSystem, RenderPipelineOptions const& options = {});

		virtual ~RenderPipeline();

		std::string const& getName() const;

		RenderPipelineOptions const& getOptions() const;

		void setExposure(float exposure);

		void setToneMapOperator(PbrToneMapOperator toneMapOperator);

		void setPbrEnvironment(PbrEnvironmentPtr environment);

		RenderTargetPtr getOutputRenderTarget();

		void resize(size_t width, size_t height);

		void addRenderPass(RenderPassPtr pass);

		void addPostEffect(ResourcePtr effect);

		virtual void render(ScenePtr scene, CameraPtr camera, glm::vec2 const& offset2d);

	};

	typedef std::shared_ptr<RenderPipeline> RenderPipelinePtr;
}