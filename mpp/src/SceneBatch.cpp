#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/matrix_interpolation.hpp>

#include "mpp/SceneBatch.h"

using namespace std;

namespace mpp
{

	SceneBatch::SceneBatch(BatchDataProviderPtr dataProvider, BatchRendererPtr renderer)
		: mDataProvider(dataProvider)
		, mRenderer(renderer)
		, mOrigin(0, 0)
		, mOffset(0, 0)
		, mScale(1, 1)
		, mAngle(0)
		, mOrbit(0)
	{
	}

	SceneBatch::~SceneBatch()
	{
	}

	void SceneBatch::setOrigin(glm::vec2 const& origin)
	{
		mOrigin = origin;
	}

	glm::vec2 const& SceneBatch::getOrigin() const
	{
		return mOrigin;
	}

	void SceneBatch::setOffset(glm::vec2 const& offset)
	{
		mOffset = offset;
	}

	glm::vec2 const& SceneBatch::getOffset() const
	{
		return mOffset;
	}

	glm::vec2 SceneBatch::getPosition() const
	{
		return mOrigin + mOffset;
	}

	void SceneBatch::setAngle(float angle)
	{
		mAngle = angle;
	}

	float SceneBatch::getAngle() const
	{
		return mAngle;
	}

	void SceneBatch::setOrbitAngle(float angle)
	{
		mOrbit = angle;
	}

	float SceneBatch::getOrbitAngle() const
	{
		return mOrbit;
	}

	void SceneBatch::setScale(glm::vec2 const& scale)
	{
		mScale = scale;
	}

	glm::vec2 const& SceneBatch::getScale() const
	{
		return mScale;
	}

	void SceneBatch::getBounds(glm::vec3& bMin, glm::vec3& bMax)
	{
		mDataProvider->getBounds(bMin, bMax);
	}

	void SceneBatch::update(float frameTime)
	{
		mDataProvider->update(frameTime);
	}

	void SceneBatch::render()
	{
		mRenderer->render();
	}
}