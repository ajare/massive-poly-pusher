#pragma once

#include "mpp/Config.h"

#include <memory>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)

namespace mpp
{
	class _MPPAPI Camera
	{
	protected:

		glm::vec3 mPosition;
		
		mutable glm::vec3 mDirection, mUp;

		mutable float mYaw, mPitch, mRoll;

		float mFov, mNear, mFar, mAspectRatio;

		mutable bool mDirty;

	protected:

		virtual void updateAngles() const;

	public:

		Camera(glm::vec3 const& position, float yaw, float pitch, float roll, float fov, float aspectRatio);

		void setFov(float fov);
		void setAspectRatio(float aspectRatio);

		float getFov() const;

		void setClipDistances(float _near, float _far);

		float getNearClipDistance() const;

		float getFarClipDistance() const;

		glm::vec3 const& getPosition() const;

		void setLookAt(glm::vec3 const& position, glm::vec3 const& target, glm::vec3 const& up = glm::vec3(0.0f, 1.0f, 0.0f));

		glm::vec3 const& getDirection() const;

		glm::vec3 const& getUp() const;

		virtual glm::mat4 getViewTransform();

		virtual glm::mat4 getProjectionTransform() const;
	};

	typedef std::shared_ptr<Camera> CameraPtr;
}