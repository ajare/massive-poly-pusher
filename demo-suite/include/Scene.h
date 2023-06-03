#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/Scene.h>

#include "ProgramOptions.h"
#include "World.h"
#include "RenderOptions.h"

class Scene
{
	mpp::ScenePtr mScene;

	std::string mSceneType;

	mpp::CameraPtr mCamera;

	mpp::ResourceManager* mResourceMgr{ nullptr };

	bool mRender{ true };

	std::vector<mpp::ResourcePtr> mResources;

private:

	virtual void setupImpl(mpp::RenderSystem* renderSystem, ProgramOptions const& options) {}

	virtual void teardownImpl() {}

	virtual mpp::CameraPtr createCamera(ProgramOptions const& options) const = 0;

protected:

	mpp::ResourceManager* getResourceManager();

	void addResource(mpp::ResourcePtr resource, bool load);

public:

	Scene(std::string const& sceneType, mpp::ResourceManager* resourceMgr);

	virtual ~Scene() = default;

	mpp::ScenePtr getScene();

	mpp::CameraPtr getCamera();

	void setRender(bool render);

	bool getRender() const;

	void setup(mpp::RenderSystem* renderSystem, ProgramOptions const& options);

	void teardown();

	virtual std::string getRenderPipelineName() const;

	virtual void update(mpp::RenderSystem* renderSystem, float frameTime) {}

	virtual void render(mpp::RenderSystem* renderSystem, World const& world, RenderOptions const& options) {}
};
