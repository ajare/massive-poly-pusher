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

		class FpsCamera : public mpp::Camera
		{
			void updateAngles() const override;

		public:

			FpsCamera(glm::vec3 const& position, float yaw, float pitch, float fov, float aspectRatio);

			void yaw(float yaw);

			void pitch(float pitch);

			void forward(float distance);

			void backward(float distance);

			void up(float distance);

			void down(float distance);

			void left(float distance);

			void right(float distance);
		};

	}
}