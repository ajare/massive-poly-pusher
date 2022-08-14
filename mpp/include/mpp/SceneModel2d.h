#pragma once

#include <vector>
#include <memory>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)

#include "mpp/Config.h"
#include "mpp/Resource.h"
#include "mpp/BatchDataProvider.h"
#include "mpp/BatchRenderer.h"
#include "mpp/ModelRenderParams.h"

namespace mpp
{
	class _MPPAPI SceneModel2d
	{
		BatchDataProviderPtr mDataProvider;

		BatchRendererPtr mRenderer;

		ResourcePtr mModel;

		RenderSystem* mRenderSystem;

		glm::vec2 mOrigin, mOffset, mScale;

		float mAngle, mOrbit;

		bool mWireframe;

		bool mVisible;

		std::shared_ptr<ModelRenderParams> mParams;

	public:

		SceneModel2d(BatchDataProviderPtr dataProvider, BatchRendererPtr renderer);

		SceneModel2d(ResourcePtr model, RenderSystem* renderSystem);

		virtual ~SceneModel2d() = default;

		void setOrigin(glm::vec2 const& origin);

		glm::vec2 const& getOrigin() const;

		void setOffset(glm::vec2 const& offset);

		glm::vec2 const& getOffset() const;

		glm::vec2 getPosition() const;

		void setAngle(float angle);

		float getAngle() const;

		void setOrbitAngle(float angle);

		float getOrbitAngle() const;

		void setScale(glm::vec2 const& scale);

		glm::vec2 const& getScale() const;

		std::shared_ptr<ModelRenderParams> getParams();

		void getBounds(glm::vec3& bMin, glm::vec3& bMax);

		void update(float frameTime);

		void render(CameraPtr camera);
	};

	typedef std::shared_ptr<SceneModel2d> SceneModel2dPtr;
}