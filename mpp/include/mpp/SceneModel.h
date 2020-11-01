#pragma once

#include <vector>
#include <memory>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)

#include "mpp/Config.h"
#include "mpp/Resource.h"

namespace mpp
{
	class _MPPAPI __declspec(align(16)) SceneModel
	{
		ResourcePtr mModel;

#pragma warning(push)
#pragma warning(disable: 4324)
		alignas(16) glm::mat4 mTransform;
#pragma warning(pop)

	public:

		explicit SceneModel(ResourcePtr model);

		virtual ~SceneModel();

		void translate(glm::vec3 const& translate);

		void rotateSelf(float angle, glm::vec3 const& axis);

		void rotateOrigin(float angle, glm::vec3 const& axis);

		void scale(glm::vec3 const& scale);

		ResourcePtr getModel() const;

		glm::mat4 const& getTransform() const;
	};

	typedef std::shared_ptr<SceneModel> SceneModelPtr;
}