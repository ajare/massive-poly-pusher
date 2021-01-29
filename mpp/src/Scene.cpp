#include "mpp/RenderSystem.h"
#include "mpp/Scene.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{
	Scene::Scene(RenderSystem* renderSystem)
		: mRenderSystem(renderSystem)
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
		vector<SceneModelPtr> inView;

		// copy only positive numbers:
		std::copy_if(mModels.begin(), mModels.end(), std::back_inserter(inView), [camera](SceneModelPtr model) 
		{
			return true; 
		});

		return inView;
	}

	vector<SceneBatchPtr> Scene::getBatchesInView()
	{
		vector<SceneBatchPtr> inView;

		auto width = mRenderSystem->getWindowWidth();
		auto height = mRenderSystem->getWindowHeight();
		
		std::copy_if(m2dBatches.begin(), m2dBatches.end(), std::back_inserter(inView), [width, height](SceneBatchPtr batch)
		{
			glm::vec3 bMin, bMax;
			batch->getBounds(bMin, bMax);

			if (bMin.x > width)
			{
				return false;
			}
			if (bMin.y > height)
			{
				return false;
			}
			if (bMax.x < 0)
			{
				return false;
			}
			if (bMax.y < 0)
			{
				return false;
			}

			return true;
		});
		
		return inView;
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