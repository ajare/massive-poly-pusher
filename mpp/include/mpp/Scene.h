#pragma once

#include <vector>
#include <memory>

#include "mpp/Config.h"
#include "mpp/Resource.h"
#include "mpp/SceneModel2d.h"
#include "mpp/SceneModel3d.h"
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

		bool mLoaded{ false };

		bool mShowModels{ true }, mShow2dBatches{ true };

		ClipRectangle mViewport;

		Colour mClearColour;

	protected:

		std::vector<SceneModel3dPtr> m3dModels;

		std::vector<std::pair<SceneModel2dPtr, int>> m2dModels;

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

		void setClearColour(Colour const& colour);

		virtual Colour getClearColour() const;

		virtual SceneModel3dPtr add3dModel(ResourcePtr model);

		virtual SceneModel2dPtr add2dModel(ResourcePtr model, int order);

		virtual SceneModel2dPtr add2dBatch(BatchDataProviderPtr dataProvider, BatchRendererPtr renderer, int order);

		void remove3dModel(SceneModel3dPtr model);

		void remove2dModel(SceneModel2dPtr model);

		void remove2dBatch(SceneModel2dPtr batch);

		virtual std::vector<SceneModel3dPtr> get3dModelsInView(CameraPtr camera);

		virtual std::vector<std::pair<SceneModel2dPtr, int>> get2dModelsInView();

		void show2dModels(bool show);

		bool show2dModels() const;

		void show3dModels(bool show);

		bool show3dModels() const;

		virtual void update(float frameTime);
	};

	typedef std::shared_ptr<Scene> ScenePtr;
}