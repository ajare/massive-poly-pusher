#pragma once

#include <vector>
#include <memory>
#include <string>
#include <map>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/vec3.hpp>
#pragma warning(pop)

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

	enum class ShadowLightType
	{
		Directional
	};

	// Shadow lights deliberately do not reuse PbrLight or the legacy light
	// API: a named shadow domain can be shared by arbitrary forward pipelines.
	struct _MPPAPI ShadowLight
	{
		ShadowLightType type{ ShadowLightType::Directional };
		glm::vec3 direction{ 0.0f, -1.0f, 0.0f };
		glm::vec3 focusPoint{ 0.0f };
	};

	struct _MPPAPI ShadowOptions
	{
		bool enabled{ false };
		ShadowLight light;
		size_t resolution{ 2048 };
		float orthoHalfWidth{ 450.0f };
		float nearPlane{ 1.0f };
		float farPlane{ 1800.0f };
		float constantBias{ 0.0008f };
		float normalBias{ 0.0025f };
		float filterRadiusTexels{ 1.0f };
	};

	struct _MPPAPI RenderPipelineOptions
	{
		RenderPipelineMode mode{ RenderPipelineMode::LegacyForward };
		float exposure{ 1.0f };
		PbrToneMapOperator toneMapOperator{ PbrToneMapOperator::Aces };
		PbrEnvironmentPtr environment;
		// Empty means this pipeline is not a shadow-domain participant.
		std::string shadowDomain;
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

		void setShadowDomain(std::string const& shadowDomain);

		RenderTargetPtr getOutputRenderTarget();

		void resize(size_t width, size_t height);

		void addRenderPass(RenderPassPtr pass);

		void addPostEffect(ResourcePtr effect);

		virtual void render(ScenePtr scene, CameraPtr camera, glm::vec2 const& offset2d);

	};

	typedef std::shared_ptr<RenderPipeline> RenderPipelinePtr;
}