#include "mpp/helper/FreeCamera.h"

namespace mpp
{
	namespace helper
	{
		using namespace glm;

		FreeCamera::FreeCamera(float aspectRatio) :
			Camera(aspectRatio)
		{
		}

		FreeCamera::FreeCamera(vec3 const& position, float aspectRatio)
			: Camera(position, aspectRatio)
		{
		}

		FreeCamera::FreeCamera(vec3 const& position, float fov, float aspectRatio)
			: Camera(position, fov, aspectRatio)
		{
		}

		FreeCamera::FreeCamera(vec3 const& position, vec3 const& direction, vec3 const& up, float aspectRatio) :
			Camera(position, direction, up, aspectRatio)
		{
		}

		FreeCamera::FreeCamera(vec3 const& position, vec3 const& direction, vec3 const& up, float fov, float aspectRatio)
			: Camera(position, direction, up, fov, aspectRatio)
		{
		}

		FreeCamera::FreeCamera(vec3 const& position, float yaw, float pitch, float roll, float aspectRatio)
			: Camera(position, yaw, pitch, roll, aspectRatio)
		{
		}
		
		FreeCamera::FreeCamera(vec3 const& position, float yaw, float pitch, float roll, float fov, float aspectRatio)
			: Camera(position, yaw, pitch, roll, fov, aspectRatio)
		{
		}

		void FreeCamera::yaw(float yaw)
		{
			quat yawQuat = angleAxis(yaw, getUp());

			mOrientation = yawQuat * mOrientation;
			normalize(mOrientation);
		}

		void FreeCamera::pitch(float pitch)
		{
			vec3 right = cross(getDirection(), getUp());
			quat pitchQuat = angleAxis(pitch, right);

			mOrientation = pitchQuat * mOrientation;
			normalize(mOrientation);
		}

		void FreeCamera::roll(float roll)
		{
			quat rollQuat = angleAxis(roll, getDirection());
			
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