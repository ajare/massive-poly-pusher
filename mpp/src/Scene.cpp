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

	SceneModel3dPtr Scene::add3dModel(ResourcePtr model, UniformCollection* uniforms)
	{
		auto sm = make_shared<SceneModel3d>(model, uniforms);
		m3dModels.push_back(sm);

		return sm;
	}

	SceneModel2dPtr Scene::add2dModel(ResourcePtr model)
	{
		auto sm = make_shared<SceneModel2d>(model, mRenderSystem);
		m2dModels.push_back(sm);

		return sm;
	}

	SceneModel2dPtr Scene::add2dModel(ResourcePtr model, shared_ptr<UniformCollection> uniforms)
	{
		auto sm = make_shared<SceneModel2d>(model, mRenderSystem, uniforms);
		m2dModels.push_back(sm);

		return sm;
	}

	SceneModel2dPtr Scene::add2dModel(ResourcePtr model, map<string, shared_ptr<UniformCollection>> const& uniforms)
	{
		auto sm = make_shared<SceneModel2d>(model, mRenderSystem, uniforms);
		m2dModels.push_back(sm);

		return sm;
	}

	SceneModel2dPtr Scene::add2dBatch(BatchDataProviderPtr dataProvider, BatchRendererPtr renderer)
	{
		auto sb = make_shared<SceneModel2d>(dataProvider, renderer);
		m2dModels.push_back(sb);

		return sb;
	}

	vector<SceneModel3dPtr> Scene::get3dModelsInView(CameraPtr camera)
	{
		vector<SceneModel3dPtr> inView;

		// Default Scene just checks if the object has its 'visible' flag set
		std::copy_if(m3dModels.begin(), m3dModels.end(), std::back_inserter(inView), [camera](SceneModel3dPtr model) 
		{
			return model->isVisible(); 
		});

		return inView;
	}

	vector<SceneModel2dPtr> Scene::get2dModelsInView()
	{
		vector<SceneModel2dPtr> inView;

		auto width = mRenderSystem->getWindowWidth();
		auto height = mRenderSystem->getWindowHeight();
		
		std::copy_if(m2dModels.begin(), m2dModels.end(), std::back_inserter(inView), [width, height](SceneModel2dPtr batch)
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

	void Scene::show2dModels(bool show)
	{
		mShow2dBatches = show;
	}

	bool Scene::show2dModels() const
	{
		return mShow2dBatches;
	}

	void Scene::show3dModels(bool show)
	{
		mShowModels = show;
	}

	bool Scene::show3dModels() const
	{
		return mShowModels;
	}

	void Scene::update(float frameTime)
	{
		for (auto batch: m2dModels)
		{
			batch->update(frameTime);
		}
	}

}