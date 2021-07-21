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
#include "mpp/UniformCollection.h"

namespace mpp
{
	class _MPPAPI SceneModel2d
	{
		enum class UniformType
		{
			None,
			Single,
			Map
		};

	private:

		BatchDataProviderPtr mDataProvider;

		BatchRendererPtr mRenderer;

		ResourcePtr mModel;

		RenderSystem* mRenderSystem;

		glm::vec2 mOrigin, mOffset, mScale;

		float mAngle, mOrbit;

		bool mWireframe;

		bool mVisible;

		std::map<std::string, std::shared_ptr<UniformCollection>> mUniforms;

		UniformType mUniformType;



	public:

		SceneModel2d(BatchDataProviderPtr dataProvider, BatchRendererPtr renderer);

		SceneModel2d(ResourcePtr model, RenderSystem* renderSystem);

		SceneModel2d(BatchDataProviderPtr dataProvider, BatchRendererPtr renderer, std::shared_ptr<UniformCollection> uniforms);

		SceneModel2d(ResourcePtr model, RenderSystem* renderSystem, std::shared_ptr<UniformCollection> uniforms);

		SceneModel2d(BatchDataProviderPtr dataProvider, BatchRendererPtr renderer, std::map<std::string, std::shared_ptr<UniformCollection>> const& uniforms);

		SceneModel2d(ResourcePtr model, RenderSystem* renderSystem, std::map<std::string, std::shared_ptr<UniformCollection>> const& uniforms);

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

		void getBounds(glm::vec3& bMin, glm::vec3& bMax);

		void update(float frameTime);

		void render();
	};

	typedef std::shared_ptr<SceneModel2d> SceneModel2dPtr;
}