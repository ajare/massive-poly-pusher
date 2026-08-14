#pragma once
#include <filesystem>
#include <memory>
#include <glm/vec3.hpp>
#include <mpp/SceneRuntime.h>
#include <mpp/resource-parsers/LegacyPipelineRuntime.h>
#include <mpp/resource-parsers/PbrPipelineRuntime.h>
#include "Scene.h"

class PackageScene final : public ::Scene
{
	std::filesystem::path mRoot;
	std::unique_ptr<mpp::resource_parsers::PbrPipelineRuntime> mPipelineRuntime;
	std::unique_ptr<mpp::resource_parsers::LegacyPipelineRuntime> mLegacyPipelineRuntime;
	std::unique_ptr<mpp::SceneRuntime> mSceneRuntime;
	mpp::RenderTargetPtr mPresentationTarget;
	std::string mGraphResource{"DemoSuite.PackageGraph"};
	mpp::SceneDocument mDocument;
	mpp::RenderSystem* mRenderer{nullptr};
	glm::vec3 mOrbitTarget{0.0f};
	float mOrbitDistance{8.0f};
	float mOrbitYaw{0.0f};
	float mOrbitPitch{0.0f};
	void updateOrbitCamera();
	void setupImpl(mpp::RenderSystem*,ProgramOptions const&) override;
	void teardownImpl() override;
	mpp::CameraPtr createCamera(ProgramOptions const&) const override;
public:
	PackageScene(mpp::ResourceManager*,std::filesystem::path root);
	std::string getRenderPipelineName() const override;
	void handleInput(InputManager*) override;
	void render(mpp::RenderSystem*,World const&,RenderOptions const&) override;
	void present(mpp::RenderSystem*);
};
