#pragma once

#include <mpp/mesh/MeshSpecification.h>

#include "Scene.h"

class ModelScene : public Scene
{
	float mTotalTime{ 0 };

	glm::vec3 mLightPosition;

	std::vector<mpp::SceneModelPtr> mModels;

private:

	void setupImpl(mpp::RenderSystem* renderSystem, ProgramOptions const& options);

	mpp::CameraPtr createCamera(ProgramOptions const& options) const;

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

	void update(mpp::RenderSystem* renderSystem, float frameTime);

	void render(mpp::RenderSystem* renderSystemr, World const& world, RenderOptions const& options);
};
