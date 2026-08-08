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
#include "mpp/RenderGraphPassFactoryRegistry.h"
#include "mpp/RenderGraphExecutor.h"
#include "mpp/RenderPipelineOutput.h"
#include "mpp/RenderOutputProcessor.h"

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
		PbrForward,
		// Explicit opt-in graph paths. Default/PbrForward retain validated manual
		// target/presentation sequences until graph output is independently proven.
		GraphPbrForward,
		GraphLegacyForward,
		XmlGraphPbrForward
	};

	enum class PbrToneMapOperator
	{
		Reinhard,
		Aces
	};

	// A pipeline-owned image-space effect. It operates on the completed scene
	// target, so PBR and legacy materials need no material-level bloom binding.
	struct _MPPAPI BloomOptions
	{
		bool enabled{ false };
		// GraphPbrForward may use an authored emissive mask in scene colour[1].
		// It falls back to threshold extract when MRT is unavailable.
		bool useMrtEmissiveMask{ false };
		float threshold{ 1.0f };
		float intensity{ 0.15f };
		uint32_t blurPasses{ 2 };
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

	enum class ShadowFilterMode
	{
		Hard,
		Pcf3x3
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
		ShadowFilterMode filterMode{ ShadowFilterMode::Pcf3x3 };
	};

	struct _MPPAPI GraphPassDebugOptions
	{
		bool shadow{ true };
		bool scene{ true };
		bool bloom{ true };
		bool presentation{ true };
	};

	struct _MPPAPI RenderPipelineOptions
	{
		RenderPipelineMode mode{ RenderPipelineMode::LegacyForward };
		float exposure{ 1.0f };
		PbrToneMapOperator toneMapOperator{ PbrToneMapOperator::Aces };
		PbrEnvironmentPtr environment;
		// Optional immutable XML graph topology for XmlGraphPbrForward.
		ResourcePtr graphTemplate;
		// Host-owned graph imports override built-in screen/shadow registrations.
		std::map<std::string, RenderTargetPtr> graphImports;
		// Named logical outputs share the XML pipeline descriptor format.
		std::vector<RenderPipelineOutput> outputs;
		// Optional emissive-mask MRT variant. XmlGraphPbrForward selects it only
		// when requested and all hardware/material output requirements validate.
		ResourcePtr graphTemplateMrt;
		BloomOptions bloom;
		GraphPassDebugOptions graphPasses;
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

		RenderTargetPtr mBloomExtractTarget;
		RenderTargetPtr mBloomPingTarget;
		RenderTargetPtr mBloomPongTarget;
		RenderTargetPtr mBloomCompositeTarget;

		std::unique_ptr<class RenderGraphTargets> mGraphTargets;
		std::unique_ptr<RenderOutputProcessor> mOutputProcessor;
		std::unique_ptr<class RenderGraphExecutor> mGraphExecutor;
		RenderGraphPassFactoryRegistry mGraphPassFactories;
		bool mWarnedMissingPbrEnvironment{ false };

		void ensureBloomTargets(size_t width, size_t height);
		void renderGraphForward(ScenePtr scene, CameraPtr camera, std::vector<SceneModel3dPtr> const& models, bool pbr);

	public:

		RenderPipeline(std::string const& name, RenderSystem* renderSystem, RenderPipelineOptions const& options = {});

		virtual ~RenderPipeline();

		std::string const& getName() const;

		RenderPipelineOptions const& getOptions() const;

		void setExposure(float exposure);

		void setToneMapOperator(PbrToneMapOperator toneMapOperator);

		void setBloomOptions(BloomOptions const& bloomOptions);

		void setGraphPassDebugOptions(GraphPassDebugOptions const& graphPasses);

		void setPbrEnvironment(PbrEnvironmentPtr environment);

		void setShadowDomain(std::string const& shadowDomain);

		RenderTargetPtr getOutputRenderTarget();
		RenderTargetPtr getGraphImageRenderTarget(GraphImageHandle image) const;
		std::vector<GraphPassExecutionStats> const& getLastGraphExecutionStats() const;
		uint64_t getOutputGeneration() const;
		std::vector<RenderPipelineOutputPlan> const& getOutputPlans() const;
		void prepareOutputs(RenderGraph const& graph, std::map<std::string, RenderTargetPtr> const& destinations);

		void resize(size_t width, size_t height);

		void addRenderPass(RenderPassPtr pass);

		void addPostEffect(ResourcePtr effect);

		virtual void render(ScenePtr scene, CameraPtr camera, glm::vec2 const& offset2d);

	};

	typedef std::shared_ptr<RenderPipeline> RenderPipelinePtr;
}