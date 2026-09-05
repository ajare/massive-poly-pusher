#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <mpp/ParticleEffectSpecification.h>
#include <mpp/ParticleSystem.h>
#include <mpp/Resource.h>

#include "ParticlePreviewPreferences.h"
#include "ParticleSpatialEditing.h"

namespace mpp
{
	class Camera;
	class RenderSystem;
	class RenderTexture;
	class ResourceManager;
	class Scene;
	class SceneModel3d;
}

namespace particle_editor
{
	class ParticlePreview
	{
		mpp::RenderSystem* mRenderSystem;
		mpp::ResourceManager* mResources;
		std::shared_ptr<mpp::Scene> mScene;
		std::shared_ptr<mpp::Camera> mCamera;
		std::array<mpp::ResourcePtr, 2> mGraphResources;
		std::array<bool, 2> mInstalledGraphs{};
		std::string mActivePipeline;
		PreviewGraph mActiveGraph{ PreviewGraph::Pbr };
		mpp::ResourcePtr mPresentationResource;
		std::shared_ptr<mpp::RenderTexture> mPresentationTexture;
		mpp::ResourcePtr mEffectResource;
		mpp::ParticleEffectHandle mEffect;
		mpp::ResourcePtr mStudioPlaneResource;
		std::array<std::shared_ptr<mpp::SceneModel3d>, 6> mStudioFaces;
		std::vector<std::shared_ptr<mpp::SceneModel3d>> mFloorGrid;
		std::array<mpp::ResourcePtr, size_t(StudioPreset::Count)> mPbrStudioMaterials;
		std::array<mpp::ResourcePtr, size_t(StudioPreset::Count)> mLegacyStudioMaterials;
		std::array<mpp::ResourcePtr, size_t(StudioPreset::Count)> mPbrGridMaterials;
		std::array<mpp::ResourcePtr, size_t(StudioPreset::Count)> mLegacyGridMaterials;
		std::vector<std::string> mOwnedResourceNames;
		ParticlePreviewPreferences mPreferences;
		StudioVolume mStudio;
		std::optional<mpp::ParticleEffectBounds> mEffectBounds;
		std::filesystem::path mPreferencesPath;
		std::optional<mpp::ParticleEffectSpecification> mInstalledSpecification;
		ParticlePreviewInputStatus mInputStatus;
		std::vector<std::string> mInputWarnings;
		std::array<bool, 2> mRetainedDepthAvailable{};
		std::string mGraphFailure;
		uint32_t mWidth{ 0 };
		uint32_t mHeight{ 0 };
		uint32_t mEffectGeneration{ 0 };
		uint32_t mRebuildCount{ 0 };
		uint32_t mLiveUpdateCount{ 0 };
		bool mPreferencesDirty{ false };
		bool mInitialised{ false };

		void createStudioResources();
		void updateStudioGeometry();
		void updateStudioVisibility();
		void applyStudioMaterials();
		void updateCamera();
		void updateLight();
		void setEffectBounds(std::optional<mpp::ParticleEffectBounds> bounds);
		void applySimulationInputs();
		void refreshInputWarnings();
		bool ensureGraphInstalled(PreviewGraph graph, std::string* failure);

	public:
		ParticlePreview(mpp::RenderSystem* renderSystem, mpp::ResourceManager* resources);
		~ParticlePreview();
		void setPreferencesPath(std::filesystem::path path);
		void initialise(std::filesystem::path const& resourceRoot, uint32_t width, uint32_t height);
		void shutdown() noexcept;
		bool install(mpp::ParticleEffectSpecification const& specification, std::string* failure = nullptr);
		bool updateLive(mpp::ParticleEffectSpecification const& specification, std::string* failure = nullptr);
		bool selectGraph(PreviewGraph graph, std::string* failure = nullptr);
		void resize(uint32_t width, uint32_t height);
		void pauseSimulation();
		void resumeSimulation();
		void stepSimulation(float deltaSeconds = 1.0f / 60.0f);
		void setSimulationTimeScale(float scale);
		bool isSimulationPaused() const;
		float simulationTimeScale() const;
		void orbitCamera(float horizontal, float vertical);
		void panCamera(float horizontal, float vertical);
		void zoomCamera(float amount);
		void focusSelection(mpp::ParticleEffectSpecification const& specification, std::optional<SpatialTarget> target);
		void frameBounds();
		void resetCamera();
		void manipulateLight(float horizontal, float vertical);
		std::optional<glm::vec2> lightViewportPosition() const;
		std::vector<ViewportOverlay> viewportOverlays(mpp::ParticleEffectSpecification const& specification,
			std::optional<SpatialTarget> selected = std::nullopt) const;
		std::optional<glm::vec2> viewportPosition(glm::vec3 world) const;
		void applyPreferences();
		void savePreferences();
		void update(float deltaSeconds);
		void render();

		mpp::ResourcePtr const& texture() const { return mPresentationResource; }
		mpp::ParticleStats const& stats() const;
		bool ready() const;
		uint32_t rebuildCount() const { return mRebuildCount; }
		uint32_t liveUpdateCount() const { return mLiveUpdateCount; }
		PreviewGraph activeGraph() const { return mActiveGraph; }
		std::string const& graphFailure() const { return mGraphFailure; }
		std::vector<std::string> const& inputWarnings() const { return mInputWarnings; }
		ParticlePreviewPreferences& preferences() { return mPreferences; }
		ParticlePreviewPreferences const& preferences() const { return mPreferences; }
		StudioVolume const& studioVolume() const { return mStudio; }
	};
}
