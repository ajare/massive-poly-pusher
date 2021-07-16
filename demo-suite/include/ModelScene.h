#pragma once

#include <mpp/mesh/MeshSpecification.h>

#include <mpp/helper/TriangleBatchRenderer.h>

#include "Scene.h"

class ModelScene : public ::Scene
{
	float mTotalTime{ 0 };

	glm::vec3 mLightPosition;

	std::vector<mpp::SceneModelPtr> mModels;

	std::vector<mpp::SceneBatchPtr> mBatches;

	std::shared_ptr<mpp::helper::TriangleBatch3DRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>> mTriangleBatch;

private:

	void setupImpl(mpp::RenderSystem* renderSystem, ProgramOptions const& options) override;

	mpp::CameraPtr createCamera(ProgramOptions const& options) const override;

	void createSharedTextures(ProgramOptions const& options);
	
	mpp::mesh::MeshSpecification createGridMeshSpecification();

	mpp::ResourcePtr createGridMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options);

	mpp::mesh::MeshSpecification createSphereMeshSpecification();

	mpp::ResourcePtr createSphereMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options);

	mpp::mesh::MeshSpecification createCylinderMeshSpecification();

	mpp::ResourcePtr createCylinderMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options);

	mpp::mesh::MeshSpecification createBoxMeshSpecification();

	mpp::ResourcePtr createBoxMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options);

	mpp::mesh::MeshSpecification createTorusMeshSpecification();

	mpp::ResourcePtr createTorusMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options);

	mpp::ResourcePtr createTorusModel(ProgramOptions const& options);

	mpp::mesh::MeshSpecification createBatch2dMeshSpecification();

	mpp::mesh::MeshSpecification createBatch3dMeshSpecification();

	void createBatchMaterials(mpp::mesh::MeshSpecification const& spec2d, mpp::mesh::MeshSpecification const& spec3d, ProgramOptions const& options);

	void createBatches(mpp::RenderSystem* renderSystem);

public:

	ModelScene(mpp::ResourceManager* resourceMgr);

	void toggle2dBatches();

	void toggleModels();

	void update(mpp::RenderSystem* renderSystem, float frameTime) override;

	void render(mpp::RenderSystem* renderSystemr, World const& world, RenderOptions const& options) override;
};
