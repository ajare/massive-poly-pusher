#pragma once

#include "Config.h"

#include "mpp/Camera.h"

namespace mpp
{
	namespace helper
	{

		class OrthoCamera : public mpp::Camera
		{
			size_t mViewWidth, mViewHeight;

			glm::vec2 mOffset;

			float mAngle;

		public:

			OrthoCamera(size_t viewWidth, size_t viewHeight);

			glm::vec2 const& getOffset() const;

			void setOffset(glm::vec2 const& offset);

			float getAngle() const;

			void setAngle(float angle);

			glm::mat4 getViewTransform() override;

			glm::mat4 getProjectionTransform() const override;

		};

	}
}