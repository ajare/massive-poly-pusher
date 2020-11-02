#pragma once

#include <vector>
#include <memory>

#include "mpp/Config.h"
#include "mpp/Resource.h"
#include "mpp/SceneModel.h"
#include "mpp/Camera.h"
#include "mpp/RenderTarget.h"
#include "mpp/Colour.h"

namespace mpp
{
	class RenderSystem;

	class _MPPAPI Scene
	{
		RenderSystem* mRenderSystem;

		RenderTargetPtr mTarget;

		std::vector<SceneModelPtr> mModels;

		std::map<std::string, CameraPtr> mCameras;

		CameraPtr mActiveCamera;

	private:

		virtual Colour getClearColour() const;

		void start();

		void finish();

	public:

		explicit Scene(RenderSystem* renderSystem);

		virtual ~Scene();

		virtual SceneModelPtr addModel(ResourcePtr model);

		virtual std::vector<SceneModelPtr> getObjectsInView(CameraPtr camera);

		void addCamera(std::string const& name, CameraPtr camera);

		void setCamera(std::string const& name);

		RenderTargetPtr getRenderTarget();

		virtual void render();
	};

	typedef std::shared_ptr<Scene> ScenePtr;
}