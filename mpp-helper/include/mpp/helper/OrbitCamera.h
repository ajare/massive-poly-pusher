#pragma once

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#pragma warning(pop)

#include "Config.h"
#include "Camera.h"

namespace mpp
{
	namespace helper
	{

		class OrbitCamera : public Camera
		{
			glm::vec3 mTargetPos, mTargetUp;

		private:

			void orbit(float angle);

			glm::quat fromLookAt(glm::vec3 const& position, glm::vec3 const& targetPos, glm::vec3 const& targetUp);

		public:

			OrbitCamera(glm::vec3 const& position, glm::vec3 const& targetPos, glm::vec3 const& targetUp);

			OrbitCamera(glm::vec3 const& position, glm::vec3 const& targetPos, glm::vec3 const& targetUp, float fov);

			void orbitClockwise(float angle);

			void orbitAnticlockwise(float angle);

			void dollyIn(float distance);

			void dollyOut(float distance);

			void trackUp(float distance);

			void trackDown(float distance);

			void trackLeft(float distance);

			void trackRight(float distance);

			void tiltLeft(float angle);

			void tiltRight(float angle);

		};

	}
}