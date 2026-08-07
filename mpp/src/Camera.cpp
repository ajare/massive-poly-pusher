#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#pragma warning(pop)

#include "mpp/Camera.h"
#include "mpp/MppException.h"

namespace mpp
{
	using namespace glm;

	Camera::Camera(vec3 const& position, float yaw, float pitch, float roll, float fov, float aspectRatio)
		: mPosition(position)
		, mYaw(yaw)
		, mPitch(pitch)
		, mRoll(roll)
		, mFov(fov)
		, mNear(0.1f)
		, mFar(1000.0f)
		, mAspectRatio(aspectRatio)
		, mDirty(true)
	{
		mDirection = vec3(0, 0, -1);
		mUp = vec3(0, 1, 0);
		updateAngles();
	}

	void Camera::setFov(float fov)
	{
		mFov = fov;
	}

	void Camera::setAspectRatio(float aspectRatio)
	{
		if(aspectRatio<=0.0f)THROW_MPP("Camera aspect ratio must be positive.",__LINE__,__FILE__,__func__);
		mAspectRatio=aspectRatio;
	}

	float Camera::getFov() const
	{
		return mFov;
	}

	void Camera::setClipDistances(float _near, float _far)
	{
		mNear = _near;
		mFar = _far;
	}

	float Camera::getNearClipDistance() const
	{
		return mNear;
	}

	float Camera::getFarClipDistance() const
	{
		return mFar;
	}

	vec3 const& Camera::getPosition() const
	{
		return mPosition;
	}

	void Camera::setLookAt(vec3 const& position, vec3 const& target, vec3 const& up)
	{
		vec3 direction = target - position;
		if (dot(direction, direction) < 0.000001f || dot(up, up) < 0.000001f)
		{
			return;
		}
		mPosition = position;
		mDirection = normalize(direction);
		mUp = normalize(up);
		mYaw = mPitch = mRoll = 0.0f;
		mDirty = false;
	}

	vec3 const& Camera::getDirection() const
	{
		updateAngles();
		return mDirection;
	}

	vec3 const& Camera::getUp() const
	{
		updateAngles();
		return mUp;
	}

	void Camera::updateAngles() const
	{
		if (mDirty)
		{
			// Pitch
			vec3 right = cross(mDirection, mUp);
			normalize(right);

			mDirection = rotate(mDirection, radians(-mPitch), right);
			normalize(mDirection);

			mUp = rotate(mUp, radians(mPitch), right);
			normalize(mUp);

			// Yaw
			mDirection = rotate(mDirection, radians(-mYaw), mUp);
			normalize(mDirection);

			// Roll
			mUp = rotate(mUp, radians(-mRoll), mDirection);
			normalize(mUp);
		}

		mPitch = mYaw = mRoll = 0.0f;
		mDirty = false;
	}

	mat4 Camera::getViewTransform()
	{
		updateAngles();

		return lookAt(mPosition, mPosition + mDirection, mUp);
	}

	mat4 Camera::getProjectionTransform() const
	{
		return perspective(radians(mFov), mAspectRatio, mNear, mFar);
	}
}