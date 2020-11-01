#pragma once

#include <vector>
#include <memory>

#include "mpp/Config.h"
#include "mpp/Resource.h"
#include "mpp/SceneModel.h"
#include "mpp/Camera.h"

namespace mpp
{
	class RenderSystem;

	class _MPPAPI Scene
	{
		RenderSystem* mRenderSystem;

		std::vector<SceneModelPtr> mModels;

		std::map<std::string, CameraPtr> mCameras;

		CameraPtr mActiveCamera;

	public:

		explicit Scene(RenderSystem* renderSystem);

		virtual ~Scene();

		SceneModelPtr addModel(ResourcePtr model);

		void addCamera(std::string const& name, CameraPtr camera);

		void setCamera(std::string const& name);

		virtual void render();
	};

	typedef std::shared_ptr<Scene> ScenePtr;
}