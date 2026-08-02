#pragma once

#include <mpp/BufferRenderer.h>

#include <mpp/mesh/MeshSpecification.h>

#include <mpp/helper/TriangleBatchRenderer.h>

#include "Scene.h"
#include "Test3dTriangleBatchDataProvider.h"
#include "ImGuiDataProvider.h"


class ModelScene : public ::Scene
{
	struct Batch2d
	{
		mpp::SceneModel2dPtr batch;
		int x, y;
		std::string label;
	};

private:

	static const uint32_t kNum2dBatches{ 6 };

private:

	float mTotalTime{ 0 };

	glm::vec3 mLightPosition;

	std::vector<mpp::SceneModel3dPtr> mModels;

	std::array<Batch2d, kNum2dBatches> m2dBatches;

	std::shared_ptr<mpp::helper::TriangleBatch3DRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>> m3dTestRenderer;

	std::shared_ptr<mpp::helper::TriangleBatch3DBufferRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>> m3dTestBufferRenderer;

	std::shared_ptr<Test3dTriangleBatchDataProvider> m3dBatchDataProvider;

	std::shared_ptr<Test3dTriangleBatchBufferDataProvider> m3dBatchBufferDataProvider;

	std::shared_ptr<ImGuiDataProvider> mImGuiDataProvider;

	mpp::BufferRenderer* mImGuiRenderer;

	// Resources
	mpp::ResourcePtr mGrid, mSphere, mCylinder, mBox, mTorus, mStatue, mPbrPreviewMaterial;

private:

	void setupImGui(mpp::RenderSystem* renderSystem, mpp::ResourceManager* resourceMgr, mpp::ScenePtr scene);

	void setupImpl(mpp::RenderSystem* renderSystem, ProgramOptions const& options) override;

	void teardownImGui();

	void teardownImpl() override;

	mpp::CameraPtr createCamera(ProgramOptions const& options) const override;

	void createSharedTextures(ProgramOptions const& options);
	
	mpp::mesh::MeshSpecification createGridMeshSpecification();

	void createGridMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options);

	mpp::mesh::MeshSpecification createSphereMeshSpecification();

	void createSphereMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options);

	mpp::mesh::MeshSpecification createCylinderMeshSpecification();

	void createCylinderMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options);

	mpp::mesh::MeshSpecification createBoxMeshSpecification();

	void createBoxMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options);

	mpp::mesh::MeshSpecification createTorusMeshSpecification();

	void createTorusMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options);

	mpp::ResourcePtr createTorusModel(ProgramOptions const& options);

	mpp::mesh::MeshSpecification createBatch2dMeshSpecification();

	mpp::mesh::MeshSpecification createBatch3dMeshSpecification();

	void createBatchMaterials(mpp::mesh::MeshSpecification const& spec2d, mpp::mesh::MeshSpecification const& spec3d, ProgramOptions const& options);

	void createBatches(mpp::RenderSystem* renderSystem);

	void updateImGui(float frameTime, mpp::RenderSystem* renderSystem);

	void renderUI(mpp::RenderSystem* renderSystem);

public:

	ModelScene(mpp::ResourceManager* resourceMgr);

	void toggle2dBatches(int batchId);

	void toggleModels();

	void toggleModel(uint32_t index);

	void handleInput(InputManager* inputMgr) override;

	void update(mpp::RenderSystem* renderSystem, float frameTime) override;

	void render(mpp::RenderSystem* renderSystemr, World const& world, RenderOptions const& options) override;
};
