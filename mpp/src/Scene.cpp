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

	SceneBatchPtr Scene::add2dBatch(BatchDataProviderPtr dataProvider, BatchRendererPtr renderer)
	{
		auto sb = make_shared<SceneBatch>(dataProvider, renderer);
		m2dBatches.push_back(sb);

		return sb;
	}

	vector<SceneModelPtr> Scene::getObjectsInView(CameraPtr camera)
	{
		return mModels;
	}

	vector<SceneBatchPtr> Scene::getBatchesInView()
	{
		return m2dBatches;
	}

	Colour Scene::getClearColour() const
	{
		return Colour::Grey25;
	}

	void Scene::update(float frameTime)
	{
		for (auto batch: m2dBatches)
		{
			batch->update(frameTime);
		}
	}

}