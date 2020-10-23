#pragma once

#include "Scene.h"

class ModelScene : public Scene
{
	enum class ModelId
	{
		None,
		Statue
	};
	
private:

	ModelId mModelId;

	mpp::ResourcePtr mStatue;

public:

	ModelScene(mpp::ResourceManager* resourceMgr);

	void setup(ProgramOptions const& options);

	void update(float frameTime);

	void render(mpp::RenderSystem* renderSystem, World const& world);
};
