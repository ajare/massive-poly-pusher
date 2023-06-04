#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/matrix_interpolation.hpp>

#include "mpp/SceneModel3d.h"

using namespace std;

namespace mpp
{

	SceneModel3d::SceneModel3d(ResourcePtr model)
		: ResourceWrangler("SceneModel2d")
		, mModel(model)
	{
		mModel->acquire(this);
		mParams = make_shared<ModelRenderParams>();
	}

	SceneModel3d::~SceneModel3d()
	{
		if (mModel)
		{
			mModel->release(this);
		}
	}

	void SceneModel3d::resetTransform()
	{
		mTransform = glm::mat4();
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

	void SceneModel3d::setModel(ResourcePtr model)
	{
		if (mModel)
		{
			mModel->release(this);
		}

		mModel = model;
	
		if (mModel)
		{
			mModel->acquire(this);
		}
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