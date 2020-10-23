#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>

#include "ProgramOptions.h"
#include "World.h"

class Scene
{
	mpp::ResourceManager* mResourceMgr{ nullptr };

	bool mRender{ true };

protected:

	mpp::ResourceManager* getResourceManager();

public:

	explicit Scene(mpp::ResourceManager* resourceMgr);

	virtual ~Scene() = default;

	void setRender(bool render);

	bool getRender() const;

	virtual void setup(ProgramOptions const& options) {}

	virtual void update(float frameTime) {}

	virtual void render(mpp::RenderSystem* renderSystem, World const& world) {}
};
