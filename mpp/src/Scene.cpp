#include "mpp/RenderSystem.h"
#include "mpp/Scene.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{
	Scene::Scene(RenderSystem* renderSystem)
		: mRenderSystem(renderSystem)
	{
		setViewport(0, 0, mRenderSystem->getWindowWidth(), mRenderSystem->getWindowHeight());
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

	void Scene::setViewport(int x, int y, size_t width, size_t height)
	{
		mViewport.x = x;
		mViewport.y = y;
		mViewport.width = (int)width;
		mViewport.height = (int)height;
	}

	ClipRectangle const& Scene::getViewport() const
	{
		return mViewport;
	}

	SceneModelPtr Scene::addModel(ResourcePtr model, UniformCollection* uniforms)
	{
		auto sm = make_shared<SceneModel>(model, uniforms);
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

		// Default Scene just checks if the object has its 'visible' flag set
		std::copy_if(mModels.begin(), mModels.end(), std::back_inserter(inView), [camera](SceneModelPtr model) 
		{
			return model->isVisible(); 
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
		return Colour::Black;
	}

	void Scene::update(float frameTime)
	{
		for (auto batch: m2dBatches)
		{
			batch->update(frameTime);
		}
	}

}