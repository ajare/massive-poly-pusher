#pragma once

#include <vector>
#include <memory>

#include "mpp/Config.h"
#include "mpp/Resource.h"
#include "mpp/SceneModel.h"
#include "mpp/SceneBatch.h"
#include "mpp/Camera.h"
#include "mpp/RenderTarget.h"
#include "mpp/Colour.h"
#include "mpp/ClipRectangle.h"

namespace mpp
{
	class RenderSystem;

	class _MPPAPI Scene
	{
		RenderSystem* mRenderSystem;

		std::vector<SceneModelPtr> mModels;

		std::vector<SceneBatchPtr> m2dBatches;

		bool mLoaded{ false };

		ClipRectangle mViewport;

	private:

		virtual void loadImpl() {}

		virtual void unloadImpl() {};

	public:

		explicit Scene(RenderSystem* renderSystem);

		virtual ~Scene();

		void load();

		void unload();

		void setViewport(int x, int y, size_t width, size_t height);

		ClipRectangle const& getViewport() const;

		virtual Colour getClearColour() const;

		virtual SceneModelPtr addModel(ResourcePtr model);

		virtual SceneBatchPtr add2dBatch(BatchDataProviderPtr dataProvider, BatchRendererPtr renderer);

		virtual std::vector<SceneModelPtr> getObjectsInView(CameraPtr camera);

		virtual std::vector<SceneBatchPtr> getBatchesInView();

		virtual void update(float frameTime);
	};

	typedef std::shared_ptr<Scene> ScenePtr;
}