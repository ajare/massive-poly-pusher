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

namespace mpp
{
	class _MPPAPI __declspec(align(16)) SceneBatch
	{
		BatchDataProviderPtr mDataProvider;

		BatchRendererPtr mRenderer;

		glm::vec2 mOrigin, mOffset, mScale;

		float mAngle, mOrbit;

	public:

		SceneBatch(BatchDataProviderPtr dataProvider, BatchRendererPtr renderer);

		virtual ~SceneBatch();

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

	typedef std::shared_ptr<SceneBatch> SceneBatchPtr;
}