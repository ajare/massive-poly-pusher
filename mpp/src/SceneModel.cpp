#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/matrix_interpolation.hpp>

#include "mpp/SceneModel.h"

using namespace std;

namespace mpp
{

	SceneModel::SceneModel(ResourcePtr model)
		: mModel(model)
	{
	}

	SceneModel::~SceneModel()
	{
	}

	void SceneModel::translate(glm::vec3 const& translate)
	{
		mTransform = glm::translate(mTransform, translate);
	}

	void SceneModel::rotateSelf(float angle, glm::vec3 const& axis)
	{
		auto rotMat = glm::axisAngleMatrix(axis, angle);
		mTransform = mTransform * rotMat;
	}

	void SceneModel::rotateOrigin(float angle, glm::vec3 const& axis)
	{
		auto rotMat = glm::axisAngleMatrix(axis, angle);
		mTransform = rotMat * mTransform;
	}

	void SceneModel::scale(glm::vec3 const& scale)
	{
		mTransform = glm::scale(mTransform, scale);
	}

	ResourcePtr SceneModel::getModel() const
	{
		return mModel;
	}

	glm::mat4 const& SceneModel::getTransform() const
	{
		return mTransform;
	}
}