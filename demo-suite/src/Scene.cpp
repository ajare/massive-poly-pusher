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

void Scene::addResource(mpp::ResourcePtr resource, bool load)
{
	resource->acquire();
	if (load)
	{
		resource->load();
	}

	mResources.push_back(resource);
}

void Scene::teardown()
{
	// Let the scene release its specific resources
	teardownImpl();

	// Release and unload any generic scene resources
	for (auto res : mResources)
	{
		res->release();

		if (res->isAvailableForUnload())
		{
			res->destroy();
		}
	}
}

string Scene::getRenderPipelineName() const
{
	return "Default";
}