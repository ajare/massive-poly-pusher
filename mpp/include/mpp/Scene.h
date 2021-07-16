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

		bool mShowModels{ true }, mShow2dBatches{ true };

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

		virtual SceneModelPtr addModel(ResourcePtr model, UniformCollection* uniforms = nullptr);

		virtual SceneBatchPtr add2dBatch(BatchDataProviderPtr dataProvider, BatchRendererPtr renderer);

		virtual std::vector<SceneModelPtr> getObjectsInView(CameraPtr camera);

		virtual std::vector<SceneBatchPtr> getBatchesInView();

		void show2dBatches(bool show);

		bool show2dBatches() const;

		void showModels(bool show);

		bool showModels() const;

		virtual void update(float frameTime);
	};

	typedef std::shared_ptr<Scene> ScenePtr;
}