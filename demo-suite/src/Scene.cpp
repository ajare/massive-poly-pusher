#include "Logger.h"
#include "Scene.h"

extern ::Logger* gLogger;

using namespace std;

Scene::Scene(string const& sceneType, mpp::ResourceManager* resourceMgr)
	: ResourceWrangler("Scene_" + sceneType)
	, mSceneType(sceneType)
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
	resource->acquire(this);
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

	// Unload all free resources
	// Note: this won't actually destroy everything, as some resources depend on others, and order matters,
	// so if we destroy resource A which is used by resource B before the resource B is destroyed, then
	// resource A will not be destroyed, because at the time it was processed, resource B still had a reference.
	// Similarly, any MppModels (which have their material implicitly as a child resource) will not be released
	// here as they are not stored in mResources: so the model will/should be released in teardownImpl() which
	// will release the material, but the material will not release the program or textures.
	for (auto res : mResources)
	{
		gLogger->message("Releasing " + res->getName());

		res->release(this);
		if (!res->isReferenced())
		{
			res->destroy();
		}
	}
}

string Scene::getRenderPipelineName() const
{
	return "Default";
}