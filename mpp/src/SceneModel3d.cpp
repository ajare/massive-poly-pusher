#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/matrix_interpolation.hpp>

#include "mpp/SceneModel3d.h"

using namespace std;

namespace mpp
{

	SceneModel3d::SceneModel3d(ResourcePtr model, UniformCollection* uniforms)
		: mModel(model)
		, mWireframe(false)
		, mVisible(true)
		, mInstanceCount(1)
		, mUniforms(uniforms)
	{
	}

	SceneModel3d::SceneModel3d(ResourcePtr model)
		: SceneModel3d(model, nullptr)
	{
	}

	SceneModel3d::~SceneModel3d()
	{
		delete mUniforms;
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

	UniformCollection* SceneModel3d::getUniformCollection()
	{
		return mUniforms;
	}

	void SceneModel3d::setWireframe(bool wireframe)
	{
		mWireframe = wireframe;
	}

	bool SceneModel3d::isWireframe() const
	{
		return mWireframe;
	}

	void SceneModel3d::setVisible(bool visible)
	{
		mVisible = visible;
	}

	bool SceneModel3d::isVisible() const
	{
		return mVisible;
	}

	void SceneModel3d::setInstanceCount(size_t instanceCount)
	{
		mInstanceCount = instanceCount;
	}

	size_t SceneModel3d::getInstanceCount() const
	{
		return mInstanceCount;
	}
}