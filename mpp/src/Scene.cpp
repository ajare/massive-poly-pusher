#include "mpp/Scene.h"
#include "mpp/RenderSystem.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{
	/*
	Scene class

	Encapsulates functionality for adding/removing models, setting lights, etc
	- Models are added at initialisation, and a 'control class' is returned, which
	  lets the user set the model's transform, and other properties.
	- The Scene renders its models by passing the control class info into RenderSystem
	  - This means that the Model/Camera/Projection matrix needs to be calculated each
		time, which isn't good for a lot of models, but this can be cached later if
		performance is an issue.
	  - The camera/view and projection matrices are taken from the active camera, which
		is also part of the scene.
		- We can add multiple cameras, and set one as active
	- Default lights can be enabled
	- This needs to work with 2d and 3d.  Different scenes will likely be needed


	To implement, copy the functionality from RenderSystem, and create renderModelBatched
	overloads which take the extra info (matrices).
	- viewPos can be extracted from the camera matrix
	*/

	Scene::Scene(RenderSystem* renderSystem)
		: mRenderSystem(renderSystem)
	{
		mTarget = renderSystem->createRenderTexture(
			"SceneTarget", 
			renderSystem->getWindowWidth(),
			renderSystem->getWindowHeight(), 
			1, 
			true);
	}

	Scene::~Scene()
	{
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

	void Scene::addCamera(string const& name, CameraPtr camera)
	{
		mCameras[name] = camera;
	}

	void Scene::setCamera(string const& name)
	{
		mActiveCamera = mCameras[name];
	}

	RenderTargetPtr Scene::getRenderTarget()
	{
		return mTarget;
	}

	Colour Scene::getClearColour() const
	{
		return Colour::Grey25;
	}

	void Scene::start()
	{
		mRenderSystem->setRenderTarget(mTarget);
	}

	void Scene::finish()
	{
		mRenderSystem->flushVertexBuffers();
	}

	void Scene::render()
	{
		start();

		auto clearColour = getClearColour();

		GL_CHECK(glClearColor(clearColour.red, clearColour.green, clearColour.blue, clearColour.alpha));
		GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

		mRenderSystem->setProjection3dPerspective(
			mActiveCamera->getFov(),
			mActiveCamera->getNearClipDistance(),
			mActiveCamera->getFarClipDistance());

		auto models = getObjectsInView(mActiveCamera);
		for (size_t pass = 0; pass < 1; ++pass)
		{
			for (auto model: models)
			{
				mRenderSystem->renderModelBatched(
					model->getModel(),
					model->getTransform(),
					mActiveCamera);
			}
		}

		finish();
	}
}