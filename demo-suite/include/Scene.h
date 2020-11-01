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

private:

	virtual void setupImpl(mpp::RenderSystem* renderSystem, ProgramOptions const& options) {}

	virtual mpp::CameraPtr createCamera(ProgramOptions const& options) = 0;

protected:

	mpp::ResourceManager* getResourceManager();

	mpp::ScenePtr getScene();

public:

	Scene(std::string const& sceneType, mpp::ResourceManager* resourceMgr);

	virtual ~Scene() = default;

	void setRender(bool render);

	bool getRender() const;

	void setup(mpp::RenderSystem* renderSystem, ProgramOptions const& options);

	virtual void update(mpp::RenderSystem* renderSystem, float frameTime) {}

	virtual void render(mpp::RenderSystem* renderSystem, World const& world, RenderOptions const& options) {}
};
