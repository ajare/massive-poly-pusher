#pragma once

#include <memory>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)

#include "mpp/Config.h"

namespace mpp
{
	class _MPPAPI Camera
	{
	protected:

		glm::vec3 mPosition;

		glm::quat mOrientation;

		float mFov, mNear, mFar, mAspectRatio;

	public:

		explicit Camera(float aspectRatio);

		Camera(glm::vec3 const& position, float aspectRatio);

		Camera(glm::vec3 const& position, float fov, float aspectRatio);

		Camera(glm::vec3 const& position, glm::quat const& orientation, float aspectRatio);

		Camera(glm::vec3 const& position, glm::quat const& orientation, float fov, float aspectRatio);

		Camera(glm::vec3 const& position, glm::vec3 const& direction, glm::vec3 const& up, float aspectRatio);

		Camera(glm::vec3 const& position, glm::vec3 const& direction, glm::vec3 const& up, float fov, float aspectRatio);

		Camera(glm::vec3 const& position, float yaw, float pitch, float roll, float aspectRatio);

		Camera(glm::vec3 const& position, float yaw, float pitch, float roll, float fov, float aspectRatio);

		void setFov(float fov);

		float getFov() const;

		void setClipDistances(float near, float far);

		float getNearClipDistance() const;

		float getFarClipDistance() const;

		glm::vec3 getPosition() const;

		glm::vec3 getDirection() const;

		glm::vec3 getUp() const;

		glm::mat4 getViewTransform() const;

		virtual glm::mat4 getProjectionTransform() const;
	};

	typedef std::shared_ptr<Camera> CameraPtr;
}