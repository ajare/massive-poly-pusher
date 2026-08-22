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
#include "mpp/AmbientOcclusion.h"
#include "mpp/RenderPass.h"
#include "mpp/Scene.h"
#include "mpp/RenderGraphPassFactoryRegistry.h"
#include "mpp/RenderGraphExecutor.h"
#include "mpp/RenderPipelineOutput.h"
#include "mpp/RenderPipelineFlow.h"
#include "mpp/RenderOutputProcessor.h"

namespace mpp
{
	class RenderSystem;
	class Program;

	struct _MPPAPI SceneOutputValidationCacheStats
	{
		uint64_t hits{ 0 };
		uint64_t misses{ 0 };
		uint64_t modelHits{ 0 };
		uint64_t modelMisses{ 0 };
		size_t uniquePrograms{ 0 };
	};

	struct _MPPAPI PbrEnvironment
	{
		ResourcePtr irradianceMap;
		ResourcePtr prefilteredSpecularMap;
		ResourcePtr brdfIntegrationLut;
		ResourcePtr backgroundMap;
		ResourcePtr environmentMap;
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
		bool ambientOcclusion{ true };
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
		AmbientOcclusionOptions ambientOcclusion;
		GraphPassDebugOptions graphPasses;
		bool debugEnvironmentCube{ false };
		// Empty means this pipeline is not a shadow-domain participant.
		std::string shadowDomain;
		// Root resource name (e.g. a PbrPipelineRuntime generation's declared root)
		// that MPP.FullscreenEffect passes prefix a bare programResource with when
		// resolving it, before falling back to a plain global lookup. Lets a
		// PbrPipelineDocument-authored pass reference one of its own LocalResources
		// by its authored name, even though that resource is only ever registered
		// under a dynamically-generated root the document can't predict itself.
		// Empty for pipelines with no document-local post-effect materials (e.g.
		// DemoSuite's XmlGraphPBR, which declares its materials as global names).
		std::string resourceRoot;
	};

	class _MPPAPI RenderPipeline
	{
		std::string mName;

		RenderSystem* mRenderSystem;

		RenderPipelineOptions mOptions;

		std::vector<RenderPassPtr> mPasses;

		RenderTargetPtr mBloomExtractTarget;
		RenderTargetPtr mBloomPingTarget;
		RenderTargetPtr mBloomPongTarget;
		RenderTargetPtr mBloomCompositeTarget;

		std::unique_ptr<class RenderGraphTargets> mGraphTargets;
		std::unique_ptr<RenderOutputProcessor> mOutputProcessor;
		std::unique_ptr<class RenderGraphExecutor> mGraphExecutor;
		RenderGraphPassFactoryRegistry mGraphPassFactories;
		bool mWarnedMissingPbrEnvironment{ false };
		uint32_t mTaaSequenceIndex{ 0 };
		bool mTaaCameraValid{ false };
		uint64_t mLastTaaFrameSerial{ 0 }, mLastCameraCutRevision{ 0 };
		glm::vec3 mLastCameraPosition{ 0.0f }, mLastCameraDirection{ 0.0f, 0.0f, -1.0f };
		float mLastCameraFov{ 0.0f }, mLastCameraAspect{ 0.0f }, mLastCameraNear{ 0.0f }, mLastCameraFar{ 0.0f };
		// Keyed by graph pass name. setPostEffectEnabled/setPostEffectParameter each
		// update this pass's entry and push the merged result, since
		// RenderGraphExecutor::setPassParameterOverrides replaces a pass's entire
		// effective parameter set rather than merging into it -- accumulating here
		// is what lets ENABLED and a numeric parameter be set independently
		// without one call clobbering the other.
		std::map<std::string, UniformCollection> mPostEffectParameterOverrides;
		bool mFlowTelemetryEnabled{ false };
		uint64_t mFlowGeneration{ 0 };
		RenderPipelineFlowSnapshotPtr mLastFlowSnapshot;
		std::shared_ptr<RenderPipelineFlowSnapshot> mPendingFlowSnapshot;

		struct ProgramOutputKey
		{
			std::shared_ptr<Program> program;
			uint64_t reflectionRevision{ 0 };
			bool operator ==(ProgramOutputKey const&) const = default;
		};
		struct SceneModelProgramCache
		{
			class Model const* model{ nullptr };
			uint64_t modelMaterialRevision{ 0 };
			uint64_t parameterRevision{ 0 };
			std::vector<std::pair<ResourcePtr, uint64_t>> materials;
			std::vector<std::shared_ptr<Program>> programs;
		};
		std::map<SceneModel3d const*, SceneModelProgramCache> mSceneModelProgramCache;
		std::vector<ProgramOutputKey> mOutputValidationPrograms;
		size_t mOutputValidationRequiredCount{ 0 };
		bool mOutputValidationKnown{ false };
		bool mOutputValidationResult{ false };
		uint64_t mOutputValidationHits{ 0 };
		uint64_t mOutputValidationMisses{ 0 };
		uint64_t mOutputValidationModelHits{ 0 };
		uint64_t mOutputValidationModelMisses{ 0 };

		bool sceneProgramsSupportOutputs(std::vector<SceneModel3dPtr> const& models, size_t requiredCount);
		void ensureBloomTargets(size_t width, size_t height);
		void renderGraphForward(ScenePtr scene, CameraPtr camera, std::vector<SceneModel3dPtr> const& models, bool pbr);
		void beginFlowSnapshot() noexcept;
		void publishFlowSnapshot() noexcept;
		void discardFlowSnapshot() noexcept;

	public:

		RenderPipeline(std::string const& name, RenderSystem* renderSystem, RenderPipelineOptions const& options = {});

		virtual ~RenderPipeline();

		std::string const& getName() const;

		RenderPipelineOptions const& getOptions() const;

		void setExposure(float exposure);

		void setToneMapOperator(PbrToneMapOperator toneMapOperator);

		void setBloomOptions(BloomOptions const& bloomOptions);

		void setAmbientOcclusionOptions(AmbientOcclusionOptions const& ambientOcclusionOptions);

		// Generic post-effect-chain tuning: enable/disable or set a named uniform
		// on the MPP.FullscreenEffect graph pass identified by `passName`. Replaces
		// BloomOptions-style typed options for chain-authored effects -- new
		// effects need no new C++ here, just a pass name and parameter name.
		void setPostEffectEnabled(std::string const& passName, bool enabled);
		void setPostEffectParameter(std::string const& passName, std::string const& parameterName, float value);

		void setGraphPassDebugOptions(GraphPassDebugOptions const& graphPasses);
		void setDebugEnvironmentCube(bool enabled);

		void setPbrEnvironment(PbrEnvironmentPtr environment);

		void setShadowDomain(std::string const& shadowDomain);

		RenderTargetPtr getOutputRenderTarget();
		RenderTargetPtr getGraphImageRenderTarget(GraphImageHandle image) const;
		std::vector<GraphPassExecutionStats> const& getLastGraphExecutionStats() const;
		std::vector<GraphPassHandle> const& getLastGraphExecutionOrder() const;
		void setFlowTelemetryEnabled(bool enabled);
		bool isFlowTelemetryEnabled() const;
		RenderPipelineFlowSnapshotPtr getLastFlowSnapshot() const;
		uint64_t getOutputGeneration() const;
		std::vector<RenderPipelineOutputPlan> const& getOutputPlans() const;
		SceneOutputValidationCacheStats getSceneOutputValidationCacheStats() const;
		void prepareOutputs(RenderGraph const& graph, std::map<std::string, RenderTargetPtr> const& destinations);

		void resize(size_t width, size_t height);

		void addRenderPass(RenderPassPtr pass);

		virtual void render(ScenePtr scene, CameraPtr camera, glm::vec2 const& offset2d);

	};

	typedef std::shared_ptr<RenderPipeline> RenderPipelinePtr;
}