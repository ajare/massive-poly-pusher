#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/matrix_interpolation.hpp>

#include "mpp/SceneModel3d.h"

using namespace std;

namespace mpp
{

	SceneModel3d::SceneModel3d(ResourcePtr model)
		: mModel(model)
	{
		mParams = make_shared<ModelRenderParams>();
	}

	void SceneModel3d::translate(glm::vec3 const& translate)
	{
		mTransform = glm::translate(mTransform, translate);
	}

	void SceneModel3d::rotateSelf(float angle, glm::vec3 const& axis)
	{
		auto rotMat = glm::axisAngleMatrix(axis, angle);
		mTransform = mTransform * rotMat;
	}

	void SceneModel3d::rotateOrigin(float angle, glm::vec3 const& axis)
	{
		auto rotMat = glm::axisAngleMatrix(axis, angle);
		mTransform = rotMat * mTransform;
	}

	void SceneModel3d::scale(glm::vec3 const& scale)
	{
		mTransform = glm::scale(mTransform, scale);
	}

	ResourcePtr SceneModel3d::getModel() const
	{
		return mModel;
	}

	glm::mat4 const& SceneModel3d::getTransform() const
	{
		return mTransform;
	}

	shared_ptr<ModelRenderParams> SceneModel3d::getParams()
	{
		return mParams;
	}
}