#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)

#include "mpp/Camera.h"

namespace mpp
{
	using namespace glm;

	Camera::Camera(float aspectRatio)
		: mPosition(vec3(0, 0, 0))
		, mOrientation()
		, mFov(90.0f)
		, mNear(0.1f)
		, mFar(1000.0f)
		, mAspectRatio(aspectRatio)
	{
	}

	Camera::Camera(vec3 const& position, float aspectRatio)
		: mPosition(position)
		, mFov(90.0f)
		, mNear(0.1f)
		, mFar(1000.0f)
		, mAspectRatio(aspectRatio)
	{
	}

	Camera::Camera(vec3 const& position, float fov, float aspectRatio)
		: mPosition(position)
		, mFov(fov)
		, mNear(0.1f)
		, mFar(1000.0f)
		, mAspectRatio(aspectRatio)
	{
	}

	Camera::Camera(vec3 const& position, quat const& orientation, float aspectRatio)
		: mPosition(position)
		, mOrientation(orientation)
		, mFov(90.0f)
		, mNear(0.1f)
		, mFar(1000.0f)
		, mAspectRatio(aspectRatio)
	{
	}

	Camera::Camera(vec3 const& position, quat const& orientation, float fov, float aspectRatio)
		: mPosition(position)
		, mOrientation(orientation)
		, mFov(fov)
		, mNear(0.1f)
		, mFar(1000.0f)
		, mAspectRatio(aspectRatio)
	{
	}

	Camera::Camera(vec3 const& position, vec3 const& direction, vec3 const& up, float aspectRatio)
		: mPosition(position)
		, mOrientation(up, direction)
		, mFov(90.0f)
		, mNear(0.1f)
		, mFar(1000.0f)
		, mAspectRatio(aspectRatio)
	{
	}

	Camera::Camera(vec3 const& position, vec3 const& direction, vec3 const& up, float fov, float aspectRatio)
		: mPosition(position)
		, mOrientation(up, direction)
		, mFov(fov)
		, mNear(0.1f)
		, mFar(1000.0f)
		, mAspectRatio(aspectRatio)
	{
	}

	Camera::Camera(vec3 const& position, float yaw, float pitch, float roll, float aspectRatio)
		: mPosition(position)
		, mOrientation(vec3(pitch, yaw, roll))
		, mFov(90.0f)
		, mNear(0.1f)
		, mFar(1000.0f)
		, mAspectRatio(aspectRatio)
	{
	}

	Camera::Camera(vec3 const& position, float yaw, float pitch, float roll, float fov, float aspectRatio)
		: mPosition(position)
		, mOrientation(vec3(pitch, yaw, roll))
		, mFov(fov)
		, mNear(0.1f)
		, mFar(1000.0f)
		, mAspectRatio(aspectRatio)
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

	mat4 Camera::getViewTransform() const
	{
		return translate(toMat4(mOrientation), -mPosition);
	}

	glm::mat4 Camera::getProjectionTransform() const
	{
		return glm::perspective(glm::radians(mFov), mAspectRatio, mNear, mFar);
	}
}