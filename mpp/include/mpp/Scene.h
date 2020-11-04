#pragma once

#include <vector>
#include <memory>

#include "mpp/Config.h"
#include "mpp/Resource.h"
#include "mpp/RenderPass.h"
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

		std::vector<RenderPassPtr> mPasses;

		std::vector<SceneModelPtr> mModels;

		std::map<std::string, CameraPtr> mCameras;

		CameraPtr mActiveCamera;

		bool mLoaded{ false };

	private:

		virtual Colour getClearColour() const;

		virtual void loadImpl() {}

		virtual void unloadImpl() {};

	public:

		explicit Scene(RenderSystem* renderSystem);

		virtual ~Scene();

		void load();

		void unload();

		virtual SceneModelPtr addModel(ResourcePtr model);

		virtual std::vector<SceneModelPtr> getObjectsInView(CameraPtr camera);

		void addCamera(std::string const& name, CameraPtr camera);

		void setCamera(std::string const& name);

		RenderTargetPtr getRenderTarget();

		virtual void render();
	};

	typedef std::shared_ptr<Scene> ScenePtr;
}