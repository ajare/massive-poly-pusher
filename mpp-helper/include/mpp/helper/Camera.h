#pragma once

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#pragma warning(pop)

#include "Config.h"

namespace mpp
{
	namespace helper
	{

		class Camera
		{
		protected:

			glm::vec3 mPosition;

			glm::quat mOrientation;

			float mFov, mNear, mFar;

		public:

			Camera();

			explicit Camera(glm::vec3 const& position);

			Camera(glm::vec3 const& position, float fov);

			Camera(glm::vec3 const& position, glm::quat const& orientation);

			Camera(glm::vec3 const& position, glm::quat const& orientation, float fov);

			Camera(glm::vec3 const& position, glm::vec3 const& direction, glm::vec3 const& up);

			Camera(glm::vec3 const& position, glm::vec3 const& direction, glm::vec3 const& up, float fov);

			Camera(glm::vec3 const& position, float yaw, float pitch, float roll);

			Camera(glm::vec3 const& position, float yaw, float pitch, float roll, float fov);

			void setFov(float fov);

			float getFov() const;

			void setClipDistances(float near, float far);

			float getNearClipDistance() const;

			float getFarClipDistance() const;

			glm::vec3 getPosition() const;

			glm::vec3 getDirection() const;

			glm::vec3 getUp() const;
		};

	}
}