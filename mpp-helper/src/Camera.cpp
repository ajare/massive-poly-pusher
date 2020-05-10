#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtx/quaternion.hpp>
#pragma warning(pop)

#include "mpp/helper/Camera.h"

namespace mpp
{
	namespace helper
	{
		using namespace glm;

		Camera::Camera() 
			: mPosition(vec3(0, 0, 0))
			, mOrientation()
			, mFov(90.0f)
			, mNear(0.1f)
			, mFar(1000.0f)
		{
		}

		Camera::Camera(vec3 const& position) 
			: mPosition(position)
			, mFov(90.0f)
			, mNear(0.1f)
			, mFar(1000.0f)
		{
		}

		Camera::Camera(vec3 const& position, float fov) 
			: mPosition(position)
			, mFov(fov)
			, mNear(0.1f)
			, mFar(1000.0f)
		{
		}

		Camera::Camera(vec3 const& position, quat const& orientation)
			: mPosition(position)
			, mOrientation(orientation)
			, mFov(90.0f)
			, mNear(0.1f)
			, mFar(1000.0f)
		{
		}

		Camera::Camera(vec3 const& position, quat const& orientation, float fov)
			: mPosition(position)
			, mOrientation(orientation)
			, mFov(fov)
			, mNear(0.1f)
			, mFar(1000.0f)
		{
		}

		Camera::Camera(vec3 const& position, vec3 const& direction, vec3 const& up)
			: mPosition(position)
			, mOrientation(up, direction)
			, mFov(90.0f)
			, mNear(0.1f)
			, mFar(1000.0f)
		{
		}

		Camera::Camera(vec3 const& position, vec3 const& direction, vec3 const& up, float fov) 
			: mPosition(position)
			, mOrientation(up, direction)
			, mFov(fov)
			, mNear(0.1f)
			, mFar(1000.0f)
		{
		}

		Camera::Camera(vec3 const& position, float yaw, float pitch, float roll)
			: mPosition(position)
			, mOrientation(vec3(pitch, yaw, roll))
			, mFov(90.0f)
			, mNear(0.1f)
			, mFar(1000.0f)
		{
		}

		Camera::Camera(vec3 const& position, float yaw, float pitch, float roll, float fov)
			: mPosition(position)
			, mOrientation(vec3(pitch, yaw, roll))
			, mFov(fov)
			, mNear(0.1f)
			, mFar(1000.0f)
		{
		}

		void Camera::setFov(float fov)
		{
			mFov = fov;
		}

		float Camera::getFov() const
		{
			return mFov;
		}

		void Camera::setClipDistances(float near, float far)
		{
			mNear = near;
			mFar = far;
		}

		float Camera::getNearClipDistance() const
		{
			return mNear;
		}

		float Camera::getFarClipDistance() const
		{
			return mFar;
		}

		vec3 Camera::getPosition() const
		{
			return mPosition;
		}

		vec3 Camera::getDirection() const
		{
			return mOrientation * vec3(0, 0, -1);
		}

		vec3 Camera::getUp() const
		{
			return mOrientation * vec3(0, 1, 0);
		}

	}
}