#include "mpp/helper/FreeCamera.h"

namespace mpp
{
	namespace helper
	{
		using namespace glm;

		FreeCamera::FreeCamera() :
			Camera()
		{
		}

		FreeCamera::FreeCamera(vec3 const& position) 
			: Camera(position)
		{
		}

		FreeCamera::FreeCamera(vec3 const& position, float fov) 
			: Camera(position, fov)
		{
		}

		FreeCamera::FreeCamera(vec3 const& position, vec3 const& direction, vec3 const& up) :
			Camera(position, direction, up)
		{
		}

		FreeCamera::FreeCamera(vec3 const& position, vec3 const& direction, vec3 const& up, float fov)
			: Camera(position, direction, up, fov)
		{
		}

		FreeCamera::FreeCamera(vec3 const& position, float yaw, float pitch, float roll)
			: Camera(position, yaw, pitch, roll)
		{
		}
		
		FreeCamera::FreeCamera(vec3 const& position, float yaw, float pitch, float roll, float fov)
			: Camera(position, yaw, pitch, roll, fov)
		{
		}

		void FreeCamera::yaw(float yaw)
		{
			quat yawQuat = angleAxis(yaw * (3.14159f / 180.0f), getUp());

			mOrientation = yawQuat * mOrientation;
			normalize(mOrientation);
		}

		void FreeCamera::pitch(float pitch)
		{
			vec3 right = cross(getDirection(), getUp());
			quat pitchQuat = angleAxis(pitch * (3.14159f / 180.0f), right);

			mOrientation = pitchQuat * mOrientation;
			normalize(mOrientation);
		}

		void FreeCamera::roll(float roll)
		{
			quat rollQuat = angleAxis(roll * (3.14159f / 180.0f), getDirection());
			
			mOrientation = rollQuat * mOrientation;
			normalize(mOrientation);
		}

		void FreeCamera::forward(float distance)
		{
			mPosition += getDirection() * distance;
		}

		void FreeCamera::backward(float distance)
		{
			mPosition -= getDirection() * distance;
		}

		void FreeCamera::up(float distance)
		{
			mPosition += getUp() * distance;
		}

		void FreeCamera::down(float distance)
		{
			mPosition -= getUp() * distance;
		}

		void FreeCamera::left(float distance)
		{
			vec3 right = cross(getDirection(), getUp());
			mPosition -= right * distance;
		}

		void FreeCamera::right(float distance)
		{
			vec3 right = cross(getDirection(), getUp());
			mPosition += right * distance;
		}

	}
}