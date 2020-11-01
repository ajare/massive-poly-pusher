#pragma once

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#pragma warning(pop)

#include "Config.h"

#include "mpp/Camera.h"

namespace mpp
{
	namespace helper
	{

		class OrbitCamera : public mpp::Camera
		{
			glm::vec3 mTargetPos, mTargetUp;

		private:

			void orbit(float angle);

			glm::quat fromLookAt(glm::vec3 const& position, glm::vec3 const& targetPos, glm::vec3 const& targetUp);

		public:

			OrbitCamera(glm::vec3 const& position, glm::vec3 const& targetPos, glm::vec3 const& targetUp, float aspectRatio);

			OrbitCamera(glm::vec3 const& position, glm::vec3 const& targetPos, glm::vec3 const& targetUp, float fov, float aspectRatio);

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