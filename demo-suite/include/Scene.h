#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>

#include "ProgramOptions.h"
#include "World.h"
#include "RenderOptions.h"

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

	virtual void setup(mpp::RenderSystem* renderSystem, ProgramOptions const& options) {}

	virtual void update(float frameTime) {}

	virtual void render(mpp::RenderSystem* renderSystem, glm::vec3 const& viewPos, glm::vec3 const& viewDir, World const& world, RenderOptions const& options) {}
};
