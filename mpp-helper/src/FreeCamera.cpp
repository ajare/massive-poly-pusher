#include "mpp/helper/FreeCamera.h"

namespace mpp
{
	namespace helper
	{
		using namespace glm;

		
		FreeCamera::FreeCamera(vec3 const& position, float yaw, float pitch, float roll, float fov, float aspectRatio)
			: Camera(position, yaw, pitch, roll, fov, aspectRatio)
		{
		}

		void FreeCamera::yaw(float yaw)
		{
			mYaw += yaw;
		}

		void FreeCamera::pitch(float pitch)
		{
			mPitch += pitch;
		}

		void FreeCamera::roll(float roll)
		{
			mRoll += roll;
		}

		void FreeCamera::forward(float distance)
		{
			mPosition += mDirection * distance;
		}

		void FreeCamera::backward(float distance)
		{
			mPosition -= mDirection * distance;
		}

		void FreeCamera::up(float distance)
		{
			mPosition += mUp * distance;
		}

		void FreeCamera::down(float distance)
		{
			mPosition -= mUp * distance;
		}

		void FreeCamera::left(float distance)
		{
			vec3 right = cross(mDirection, mUp);
			mPosition -= right * distance;
		}

		void FreeCamera::right(float distance)
		{
			vec3 right = cross(mDirection, mUp);
			mPosition += right * distance;
		}

	}
}