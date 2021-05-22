#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/matrix_interpolation.hpp>

#include "mpp/SceneModel.h"

using namespace std;

namespace mpp
{

	SceneModel::SceneModel(ResourcePtr model, UniformCollection* uniforms)
		: mModel(model)
		, mWireframe(false)
		, mVisible(true)
		, mInstanceCount(1)
		, mUniforms(uniforms)
	{
	}

	SceneModel::SceneModel(ResourcePtr model)
		: SceneModel(model, nullptr)
	{
	}

	SceneModel::~SceneModel()
	{
		delete mUniforms;
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

	UniformCollection* SceneModel::getUniformCollection()
	{
		return mUniforms;
	}

	void SceneModel::setWireframe(bool wireframe)
	{
		mWireframe = wireframe;
	}

	bool SceneModel::isWireframe() const
	{
		return mWireframe;
	}

	void SceneModel::setVisible(bool visible)
	{
		mVisible = visible;
	}

	bool SceneModel::isVisible() const
	{
		return mVisible;
	}

	void SceneModel::setInstanceCount(size_t instanceCount)
	{
		mInstanceCount = instanceCount;
	}

	size_t SceneModel::getInstanceCount() const
	{
		return mInstanceCount;
	}
}