#include "Scene.h"

using namespace std;

Scene::Scene(string const& sceneType, mpp::ResourceManager* resourceMgr)
	: mSceneType(sceneType)
	, mResourceMgr(resourceMgr)
{
}

mpp::ResourceManager* Scene::getResourceManager()
{
	return mResourceMgr;
}

void Scene::setRender(bool render)
{
	mRender = render;
}

bool Scene::getRender() const
{
	return mRender;
}

mpp::ScenePtr Scene::getScene()
{
	return mScene;
}

mpp::CameraPtr Scene::getCamera()
{
	return mCamera;
}

void Scene::setup(mpp::RenderSystem* renderSystem, ProgramOptions const& options)
{
	mScene = renderSystem->createScene(mSceneType);
	mCamera = createCamera(options);

	setupImpl(renderSystem, options);
}

void Scene::teardown()
{
	teardownImpl();
}

string Scene::getRenderPipelineName() const
{
	return "Default";
}