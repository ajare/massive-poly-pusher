#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <mpp/ParticleSystem.h>
#include <mpp/TrailSystem.h>
#include <mpp/SceneRuntime.h>

#include "Scene.h"

class ParticleScene final : public ::Scene
{
	std::filesystem::path mResourceRoot;
	std::unique_ptr<mpp::SceneRuntime> mSceneRuntime;
	mpp::RenderSystem* mRenderer{ nullptr };
	mpp::RenderTargetPtr mPresentationTarget;
	mpp::ParticleEffectHandle mDemoEffect;
	mpp::ParticleEffectHandle mSerializedEffect;
	mpp::ParticleEffectHandle mStressEffect;
	mpp::TrailHandle mDemoTrail;
	std::string mPipelineName{ "ParticlePbr" };
	bool mPbr{ true };
	bool mStressMode{ false };

	void setupImpl(mpp::RenderSystem*, ProgramOptions const&) override;
	void teardownImpl() override;
	mpp::CameraPtr createCamera(ProgramOptions const&) const override;
	void createDemoEffect();
	void enableStressMode();

public:
	ParticleScene(mpp::ResourceManager*, std::filesystem::path resourceRoot);
	std::string getRenderPipelineName() const override;
	void handleInput(InputManager*) override;
	void render(mpp::RenderSystem*, World const&, RenderOptions const&) override;
	void present(mpp::RenderSystem*);
	std::vector<std::string> getOverlayLines() const override;
};
