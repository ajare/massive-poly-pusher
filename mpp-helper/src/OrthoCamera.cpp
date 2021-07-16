#include "mpp/helper/OrthoCamera.h"

namespace mpp
{
	namespace helper
	{
		using namespace glm;

		OrthoCamera::OrthoCamera(size_t viewWidth, size_t viewHeight)
			: Camera(vec3(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 0.0f, (float)viewWidth / viewHeight)
			, mViewWidth(viewWidth)
			, mViewHeight(viewHeight)
			, mOffset(0.0f, 0.0f)
			, mAngle(0.0f)
		{
		}

		glm::mat4 OrthoCamera::getViewTransform()
		{
			mat4 m;
			
			m = translate(m, vec3(mOffset.x, 0.0f, mOffset.y));
			return rotate(m, mAngle, vec3(0.0f, 1.0f, 0.0f));
		}

		glm::mat4 OrthoCamera::getProjectionTransform() const
		{
			return glm::ortho(0, (int)mViewWidth, 0, (int)mViewHeight);
		}
	}
}