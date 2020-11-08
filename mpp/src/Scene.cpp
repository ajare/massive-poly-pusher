#include "mpp/Scene.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{
	Scene::Scene()
	{
	}

	Scene::~Scene()
	{
		unload();
	}

	void Scene::load()
	{
		if (!mLoaded)
		{
			loadImpl();
			mLoaded = true;
		}
	}

	void Scene::unload()
	{
		if (mLoaded)
		{
			unloadImpl();
			mLoaded = false;
		}
	}

	SceneModelPtr Scene::addModel(ResourcePtr model)
	{
		auto sm = make_shared<SceneModel>(model);
		mModels.push_back(sm);

		return sm;
	}

	vector<SceneModelPtr> Scene::getObjectsInView(CameraPtr camera)
	{
		return mModels;
	}

	Colour Scene::getClearColour() const
	{
		return Colour::Grey25;
	}

}