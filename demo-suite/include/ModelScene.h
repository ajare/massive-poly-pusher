#pragma once

#include <mpp/mesh/MeshSpecification.h>

#include "Scene.h"

class ModelScene : public Scene
{
	struct ModelData
	{
		mpp::ResourcePtr model;
		glm::vec3 position;
		glm::vec3 scale;
		float angle;
	};
	
private:

	std::vector<ModelData> mModels;

	float mTotalTime{ 0 };

private:

	void createSharedTextures(ProgramOptions const& options);
	
	mpp::mesh::MeshSpecification createGridMeshSpecification();

	mpp::ResourcePtr createGridMaterial(mpp::mesh::MeshSpecification const& meshSpec);

	mpp::mesh::MeshSpecification createSphereMeshSpecification();

	mpp::ResourcePtr createSphereMaterial(mpp::mesh::MeshSpecification const& meshSpec);

	mpp::mesh::MeshSpecification createCylinderMeshSpecification();

	mpp::ResourcePtr createCylinderMaterial(mpp::mesh::MeshSpecification const& meshSpec);

	mpp::mesh::MeshSpecification createBoxMeshSpecification();

	mpp::ResourcePtr createBoxMaterial(mpp::mesh::MeshSpecification const& meshSpec);

	mpp::mesh::MeshSpecification createTorusMeshSpecification();

	mpp::ResourcePtr createTorusMaterial(mpp::mesh::MeshSpecification const& meshSpec);

	mpp::ResourcePtr createTorusModel();

public:

	ModelScene(mpp::ResourceManager* resourceMgr);

	void setup(ProgramOptions const& options);

	void update(float frameTime);

	void render(mpp::RenderSystem* renderSystem, glm::vec3 const& viewPos, glm::vec3 const& viewDir, World const& world, RenderOptions const& options);
};
