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

		std::vector<SceneModel3dPtr> m3dModels;

		std::vector<SceneModel2dPtr> m2dModels;

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

		virtual SceneModel3dPtr add3dModel(ResourcePtr model, UniformCollection* uniforms = nullptr);

		virtual SceneModel2dPtr add2dModel(ResourcePtr model);

		virtual SceneModel2dPtr add2dModel(ResourcePtr model, std::shared_ptr<UniformCollection> uniforms);

		virtual SceneModel2dPtr add2dModel(ResourcePtr model, std::map<std::string, std::shared_ptr<UniformCollection>> const& uniforms);

		virtual SceneModel2dPtr add2dBatch(BatchDataProviderPtr dataProvider, BatchRendererPtr renderer);

		virtual std::vector<SceneModel3dPtr> get3dModelsInView(CameraPtr camera);

		virtual std::vector<SceneModel2dPtr> get2dModelsInView();

		void show2dModels(bool show);

		bool show2dModels() const;

		void show3dModels(bool show);

		bool show3dModels() const;

		virtual void update(float frameTime);
	};

	typedef std::shared_ptr<Scene> ScenePtr;
}