#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "mpp/helper/OrbitCamera.h"

namespace mpp
{
	namespace helper
	{
		using namespace glm;

		OrbitCamera::OrbitCamera(vec3 const& position, vec3 const& targetPos, vec3 const& targetUp, float aspectRatio)
			: Camera(position, fromLookAt(position, targetPos, targetUp), aspectRatio)
		{
			mTargetPos = targetPos;
			mTargetUp = targetUp;
		}

		OrbitCamera::OrbitCamera(vec3 const& position, vec3 const& targetPos, vec3 const& targetUp, float fov, float aspectRatio)
			: Camera(position, fromLookAt(position, targetPos, targetUp), fov, aspectRatio)
		{
			mTargetPos = targetPos;
			mTargetUp = targetUp;
		}

		void OrbitCamera::orbit(float angle)
		{
			mPosition = rotate(mPosition, radians(angle), mTargetUp);
			mOrientation = fromLookAt(mPosition, mTargetPos, mTargetUp);
		}

		quat OrbitCamera::fromLookAt(vec3 const& position, vec3 const& targetPos, vec3 const& targetUp)
		{
			// Calculate camera up from target up
			vec3 cameraDir, cameraSide, cameraUp;

			cameraDir = normalize(targetPos - position);
			cameraSide = cross(cameraDir, targetUp);
			cameraUp = cross(cameraSide, cameraDir);

			return conjugate(quat(lookAt(position, targetPos, cameraUp)));
		}

		void OrbitCamera::orbitClockwise(float angle)
		{
			orbit(-angle);
		}

		void OrbitCamera::orbitAnticlockwise(float angle)
		{
			orbit(angle);
		}

		void OrbitCamera::dollyIn(float distance)
		{
			vec3 d = mTargetPos - mPosition;

			if (d.length() < 0.01f)
			{
				return;
			}
			
			vec3 dolly = getDirection() * distance;
			mPosition += dolly;
		}

		void OrbitCamera::dollyOut(float distance)
		{
			vec3 dolly = getDirection() * distance;
			mPosition -= dolly;
		}

		void OrbitCamera::trackUp(float distance)
		{
			// Both position and target move along up vector
			mPosition += mTargetUp * distance;
			mTargetPos += mTargetUp * distance;
		}

		void OrbitCamera::trackDown(float distance)
		{
			// Both position and target move along up vector
			mPosition -= mTargetUp * distance;
			mTargetPos -= mTargetUp * distance;
		}

		void OrbitCamera::trackLeft(float distance)
		{
			// Both position and target move along side vector
			vec3 cameraDir, cameraSide;

			cameraDir = normalize(mTargetPos - mPosition);
			cameraSide = cross(cameraDir, mTargetUp);

			mPosition -= cameraSide * distance;
			mTargetPos -= cameraSide * distance;
		}

		void OrbitCamera::trackRight(float distance)
		{
			// Both position and target move along side vector
			vec3 cameraDir, cameraSide;

			cameraDir = normalize(mTargetPos - mPosition);
			cameraSide = cross(cameraDir, mTargetUp);

			mPosition += cameraSide * distance;
			mTargetPos += cameraSide * distance;
		}

		void OrbitCamera::tiltLeft(float angle)
		{
			quat rollQuat = angleAxis(radians(-angle), getDirection());

			mOrientation = rollQuat * mOrientation;
			normalize(mOrientation);
		}

		void OrbitCamera::tiltRight(float angle)
		{
			quat rollQuat = angleAxis(radians(angle), getDirection());

			mOrientation = rollQuat * mOrientation;
			normalize(mOrientation);
		}
	}
}