#pragma once

#include <mpp/mesh/MeshSpecification.h>

#include <mpp/helper/LineBatchRenderer.h>
#include <mpp/helper/QuadBatchRenderer.h>

#include "Scene.h"
#include "TrefoilWindow.h"
#include "Control.h"
#include "LineBatchDataProvider.h"
#include "QuadBatchDataProvider.h"

class ModelScene : public ::Scene
{
	float mTotalTime{ 0 };

	glm::vec3 mLightPosition;

	std::vector<mpp::SceneModelPtr> mModels;

	// Window schematic lines
	std::shared_ptr<TrefoilWindowDataProvider> mLineDataProvider;

	std::shared_ptr<mpp::helper::LineBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>> mLineRenderer;

	// UI control lines
	std::shared_ptr<ControlLinesDataProvider> mControlLinesDataProvider;

	std::shared_ptr<mpp::helper::LineBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>> mControlsLineRenderer;

	// UI control handles
	std::shared_ptr<ControlHandlesDataProvider> mControlHandlesDataProvider;

	std::shared_ptr<CircleDataProvider> mControlHandlesCircleDataProvider;

	std::shared_ptr<CircleRenderer> mControlHandlesCircleRenderer;

	std::shared_ptr<mpp::helper::QuadBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>> mControlHandlesRenderer;

	// Window
	TrefoilWindow* mTrefoilWindow;

	// UI Controls
	std::vector<Control*> mControls;

private:

	void setupImpl(mpp::RenderSystem* renderSystem, ProgramOptions const& options);

	void createControls();

	mpp::CameraPtr createCamera(ProgramOptions const& options) const;

	void createSharedTextures(ProgramOptions const& options);
	
	mpp::mesh::MeshSpecification createMeshSpecification();

	mpp::ResourcePtr createMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options);

public:

	ModelScene(mpp::ResourceManager* resourceMgr);

	~ModelScene();

	void update(mpp::RenderSystem* renderSystem, float frameTime);

	void render(mpp::RenderSystem* renderSystemr, World const& world, RenderOptions const& options);
};
