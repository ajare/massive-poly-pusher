#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/matrix_interpolation.hpp>

#include "mpp/SceneModel2d.h"
#include "mpp/RenderSystem.h"

using namespace std;

namespace mpp
{

	SceneModel2d::SceneModel2d(BatchDataProviderPtr dataProvider, BatchRendererPtr renderer)
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
		, mUniformType(UniformType::None)
	{
	}

	SceneModel2d::SceneModel2d(ResourcePtr model, RenderSystem* renderSystem)
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
		, mUniformType(UniformType::None)
	{
	}

	SceneModel2d::SceneModel2d(BatchDataProviderPtr dataProvider, BatchRendererPtr renderer, std::shared_ptr<UniformCollection> uniforms)
		: SceneModel2d(dataProvider, renderer)
	{
		mUniforms["_"] = uniforms;
		mUniformType = UniformType::Single;
	}

	SceneModel2d::SceneModel2d(ResourcePtr model, RenderSystem* renderSystem, std::shared_ptr<UniformCollection> uniforms)
		: SceneModel2d(model, renderSystem)
	{
		mUniforms["_"] = uniforms;
		mUniformType = UniformType::Map;
	}

	SceneModel2d::SceneModel2d(BatchDataProviderPtr dataProvider, BatchRendererPtr renderer, std::map<std::string, std::shared_ptr<UniformCollection>> const& uniforms)
		: SceneModel2d(dataProvider, renderer)
	{
		mUniforms = uniforms;
		mUniformType = UniformType::Single;
	}

	SceneModel2d::SceneModel2d(ResourcePtr model, RenderSystem* renderSystem, std::map<std::string, std::shared_ptr<UniformCollection>> const& uniforms)
		: SceneModel2d(model, renderSystem)
	{
		mUniforms = uniforms;
		mUniformType = UniformType::Map;
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
			switch (mUniformType)
			{
			case UniformType::None:
				mRenderSystem->renderModelImmediate(*static_cast<Model*>(mModel.get()), true);
				break;
			case UniformType::Single:
				mRenderSystem->renderModelImmediate(*static_cast<Model*>(mModel.get()), true, mUniforms["_"]);
				break;
			case UniformType::Map:
				mRenderSystem->renderModelImmediate(*static_cast<Model*>(mModel.get()), true, mUniforms);
				break;

			}
		}
	}
}