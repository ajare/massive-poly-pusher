#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include <mpp/ParticleEffectSpecification.h>
#include <mpp/ParticleSystem.h>
#include <mpp/Resource.h>

namespace mpp
{
	class Camera;
	class RenderSystem;
	class RenderTexture;
	class ResourceManager;
	class Scene;
}

namespace particle_editor
{
	class ParticlePreview
	{
		mpp::RenderSystem* mRenderSystem;
		mpp::ResourceManager* mResources;
		std::shared_ptr<mpp::Scene> mScene;
		std::shared_ptr<mpp::Camera> mCamera;
		mpp::ResourcePtr mGraphResource;
		mpp::ResourcePtr mPresentationResource;
		std::shared_ptr<mpp::RenderTexture> mPresentationTexture;
		mpp::ResourcePtr mEffectResource;
		mpp::ParticleEffectHandle mEffect;
		uint32_t mWidth{ 0 };
		uint32_t mHeight{ 0 };
		uint32_t mEffectGeneration{ 0 };
		bool mInitialised{ false };

	public:
		ParticlePreview(mpp::RenderSystem* renderSystem, mpp::ResourceManager* resources);
		~ParticlePreview();
		void initialise(std::filesystem::path const& resourceRoot, uint32_t width, uint32_t height);
		void shutdown() noexcept;
		bool install(mpp::ParticleEffectSpecification const& specification, std::string* failure = nullptr);
		void resize(uint32_t width, uint32_t height);
		void pauseSimulation();
		void resumeSimulation();
		void stepSimulation(float deltaSeconds = 1.0f / 60.0f);
		void setSimulationTimeScale(float scale);
		bool isSimulationPaused() const;
		float simulationTimeScale() const;
		void render();

		mpp::ResourcePtr const& texture() const { return mPresentationResource; }
		mpp::ParticleStats const& stats() const;
		bool ready() const { return mInitialised && bool(mEffectResource); }
	};
}
