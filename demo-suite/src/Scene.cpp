#include "Scene.h"

using namespace mpp;

Scene::Scene(mpp::ResourceManager* resourceMgr)
	: mResourceMgr(resourceMgr)
{
}

ResourceManager* Scene::getResourceManager()
{
	return mResourceMgr;
}

void Scene::setRender(bool render)
{
	mRender = render;
}

bool Scene::getRender() const
{
	return mRender;
}