#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/matrix_interpolation.hpp>

#include "mpp/SceneModel2d.h"
#include "mpp/RenderSystem.h"

using namespace std;

namespace mpp
{

	SceneModel2d::SceneModel2d(BatchDataProviderPtr dataProvider, BatchRendererPtr renderer, bool hasUniforms)
		: mDataProvider(dataProvider)
		, mRenderer(renderer)
		, mModel(nullptr)
		, mRenderSystem(nullptr)
		, mOrigin(0, 0)
		, mOffset(0, 0)
		, mScale(1, 1)
		, mAngle(0)
		, mOrbit(0)
		, mWireframe(false)
		, mVisible(true)
	{
		if (hasUniforms)
		{
			mUniforms = make_shared<UniformCollection>();
		}
	}

	SceneModel2d::SceneModel2d(ResourcePtr model, RenderSystem* renderSystem, bool hasUniforms)
		: mDataProvider(nullptr)
		, mRenderer(nullptr)
		, mModel(model)
		, mRenderSystem(renderSystem)
		, mOrigin(0, 0)
		, mOffset(0, 0)
		, mScale(1, 1)
		, mAngle(0)
		, mOrbit(0)
		, mWireframe(false)
		, mVisible(true)
	{
		if (hasUniforms)
		{
			mUniforms = make_shared<UniformCollection>();
		}
	}

	SceneModel2d::~SceneModel2d()
	{
	}

	void SceneModel2d::setOrigin(glm::vec2 const& origin)
	{
		mOrigin = origin;
	}

	glm::vec2 const& SceneModel2d::getOrigin() const
	{
		return mOrigin;
	}

	void SceneModel2d::setOffset(glm::vec2 const& offset)
	{
		mOffset = offset;
	}

	glm::vec2 const& SceneModel2d::getOffset() const
	{
		return mOffset;
	}

	glm::vec2 SceneModel2d::getPosition() const
	{
		return mOrigin + mOffset;
	}

	void SceneModel2d::setAngle(float angle)
	{
		mAngle = angle;
	}

	float SceneModel2d::getAngle() const
	{
		return mAngle;
	}

	void SceneModel2d::setOrbitAngle(float angle)
	{
		mOrbit = angle;
	}

	float SceneModel2d::getOrbitAngle() const
	{
		return mOrbit;
	}

	void SceneModel2d::setScale(glm::vec2 const& scale)
	{
		mScale = scale;
	}

	glm::vec2 const& SceneModel2d::getScale() const
	{
		return mScale;
	}

	shared_ptr<UniformCollection> SceneModel2d::getUniformCollection()
	{
		return mUniforms;
	}

	void SceneModel2d::getBounds(glm::vec3& bMin, glm::vec3& bMax)
	{
		if (mDataProvider)
		{
			mDataProvider->getBounds(bMin, bMax);
		}
		else
		{
			// TODO
			//static_cast<Model*>(mModel.get())->get
		}
	}

	void SceneModel2d::update(float frameTime)
	{
		if (mDataProvider && mDataProvider->update(frameTime))
		{
			mRenderer->update(mDataProvider->getNumPrimitives());
		}
	}

	void SceneModel2d::render()
	{
		if (mRenderer)
		{
			mRenderer->render();
		}
		else if (mModel)
		{
			mRenderSystem->renderModelImmediate(*static_cast<Model*>(mModel.get()), true, mUniforms);
		}
	}
}