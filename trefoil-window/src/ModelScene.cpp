#include <glm/gtx/rotate_vector.hpp>

#include <mpp/MppModelStream.h>
#include <mpp/SphereModelStream.h>
#include <mpp/GridModelStream.h>
#include <mpp/CylinderModelStream.h>
#include <mpp/BoxModelStream.h>
#include <mpp/ProgrammaticModelStream.h>
#include <mpp/ProgrammaticMaterialStream.h>
#include <mpp/ProgrammaticTextureStream.h>
#include <mpp/ProgrammaticSamplerStream.h>
#include <mpp/ResourceStreamSerializer.h>

#include <mpp/resource-parsers/FileTextureStream.h>
#include <mpp/resource-parsers/FileProgramStream.h>
#include <mpp/resource-parsers/FileMaterialStream.h>
#include <mpp/resource-parsers/FileStringStream.h>

#include <mpp/helper/FreeCamera.h>

#include "ModelScene.h"
#include "Helper.h"

using namespace std;
using namespace mpp;

ModelScene::ModelScene(mpp::ResourceManager* resourceMgr)
	: Scene("Default", resourceMgr)
	, mLightPosition(0, 256, 256)
	, mTrefoilWindow(nullptr)
{
}

ModelScene::~ModelScene()
{
	delete mTrefoilWindow;
}

void ModelScene::createSharedTextures(ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	// Create a Sampler resource.  This holds parameters that shaders use when sampling textures.
	auto samplerStream = new ProgrammaticSamplerStream(resourceMgr);
	samplerStream->setFiltering(mpp::SamplerParams::MinFilter::Linear, mpp::SamplerParams::MagFilter::Linear);
	resourceMgr->declareResource("Default.Sampler", ResourceStreamPtr(samplerStream));

	// Create texture with sampler.
	auto textureStream = new ProgrammaticTextureStream(resourceMgr);
	textureStream->setTarget(TextureTarget::Texture2D);
	textureStream->setFile(options.resourceLocation + "marble_texture4662.jpg", loadImage);
	textureStream->enableMipMaps(true);
	textureStream->setSampler("Default.Sampler");
	resourceMgr->declareResource("Marble.Texture", ResourceStreamPtr(textureStream));
}

mesh::MeshSpecification ModelScene::createMeshSpecification()
{
	mesh::MeshSpecification meshSpec(mesh::Primitive::Type::Triangles);

	mesh::VertexBufferAttributeLayout* attribLayout = meshSpec.createVertexBufferAttributeLayout(false);
	attribLayout->createAttribute(mesh::Vertex::Component::Position3, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::Normal3, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::Float, true);

	meshSpec.setStorageType(mesh::VertexBufferStorageType::Static);
	meshSpec.setIndexedVertices(true);

	return meshSpec;
}

mpp::ResourcePtr ModelScene::createMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	auto materialStream = new ProgrammaticMaterialStream(resourceMgr);
	materialStream->setProgram2d(false);
	materialStream->setMeshSpecification(meshSpec);
	materialStream->setTexture("TEX1", "Marble.Texture");

	auto res = resourceMgr->declareResource("Cylinder.Material", ResourceStreamPtr(materialStream));
	res->load();

	return res;
}

void ModelScene::setupImpl(mpp::RenderSystem* renderSystem, ProgramOptions const& options)
{
	mControlOffset = glm::vec2(renderSystem->getWindowWidth() / 4, 80);
	mWindowHeight = renderSystem->getWindowHeight();

	// Create trefoil
	mTrefoilWindow = new TrefoilWindow(2);

	/*
	mTrefoilWindow->setPaneBaseOffset(10);
	mTrefoilWindow->setPaneUpperBufferHeight(50);
	mTrefoilWindow->setPaneHeight(250);
	//mTrefoilWindow->setPaneSpacing(20);

	auto& upperTrefoil = mTrefoilWindow->getUpperTrefoil();
	upperTrefoil.foilOffset = 0;
	upperTrefoil.distance = 40;
	upperTrefoil.radius = 40;

	auto& paneTrefoil = mTrefoilWindow->getPaneTrefoil();

	paneTrefoil.numFoils = 3;
	paneTrefoil.distance = 40;
	paneTrefoil.radius = 40;
	*/
	
	mTrefoilWindow->load("window.settings");

	createControls(renderSystem);

	// Set scene up
	auto mppScene = getScene();
	mppScene->setViewport(renderSystem->getWindowWidth() / 2, 0, renderSystem->getWindowWidth() / 2, renderSystem->getWindowHeight());

	// Create trefoil models
	auto resourceMgr = getResourceManager();

	createSharedTextures(options);

	auto meshSpec = createMeshSpecification();
	createMaterial(meshSpec, options);

	mpp::helper::LineBatchRendererParams lineParams
	{
		true,
		true,
		false
	};

	// Window schematic
	mLineDataProvider = make_shared<TrefoilWindowDataProvider>(renderSystem, mTrefoilWindow);

	mLineRenderer = make_shared<mpp::helper::LineBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>>(
		"WindowLines",
		lineParams,
		mLineDataProvider,
		renderSystem,
		resourceMgr);

	mLineRenderer->create();
	auto batch = mppScene->add2dBatch(mLineDataProvider, mLineRenderer);
	batch->setOrigin(mControlOffset);

	// UI control lines
	mControlLinesDataProvider = make_shared<ControlLinesDataProvider>(renderSystem, mControls);

	mControlsLineRenderer = make_shared<mpp::helper::LineBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>>(
		"ControlLines",
		lineParams,
		mControlLinesDataProvider,
		renderSystem,
		resourceMgr);

	mControlsLineRenderer->create();
	batch = mppScene->add2dBatch(mControlLinesDataProvider, mControlsLineRenderer);
	batch->setOrigin(mControlOffset);

	// Round control handles
	/*
	mControlHandlesCircleDataProvider = make_shared<CircleDataProvider>(8, mControls);

	mControlHandlesCircleRenderer = make_shared<CircleRenderer>(
		"ControlHandleCircles",
		mControlHandlesCircleDataProvider,
		renderSystem,
		resourceMgr);

	mpp::helper::QuadBatchRendererParams quadParams(
		mpp::QuadBatchOptions::PrimitiveOptions::Triangles,
		true,
		true,
		true,
		false,
		true,
		16,
		16,
		true,
		16,
		mControlHandlesCircleRenderer
	);
	*/
	// Square control handles
	
	mpp::helper::QuadBatchRendererParams quadParams(
		mpp::QuadBatchOptions::PrimitiveOptions::Auto,
		true,  // fixed texcoords
		true,  // fixed colour (no colour, in fact)
		false, // don't use vertex colours
		true,  // use diffuse colour
		false, // don't rotate
		8,    // width
		8,    // height
		true,  // square
		16);   // 16-bit indices
	

	mControlHandlesDataProvider = make_shared<ControlHandlesDataProvider>(renderSystem, mControls);

	mControlHandlesRenderer = make_shared<mpp::helper::QuadBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>>(
		"ControlHandles",
		quadParams,
		mControlHandlesDataProvider,
		renderSystem,
		resourceMgr);

	mControlHandlesRenderer->create();
	batch = mppScene->add2dBatch(mControlHandlesDataProvider, mControlHandlesRenderer);
	batch->setOrigin(mControlOffset);

	// 3D model
	mpp::helper::TriangleBatchRendererParams triParams
	{
		false,
		true,
		true,
		false
	};

	mSchematicDataProvider = make_shared<Trefoil3DDataProvider>(mTrefoilWindow);

	mModelRenderer = make_shared<mpp::helper::TriangleBatch3DRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>>(
		"Trefoil3D",
		triParams,
		mSchematicDataProvider,
		nullptr, //resourceMgr->getResource("Marble.Texture"),		
		renderSystem, resourceMgr);

	mModelRenderer->create();

	mModel = getScene()->addModel(mModelRenderer->getModel());
	mModels.push_back(mModel);
	auto modelScale = 0.50f;
	mModel->translate(glm::vec3(0, -40, 0));
	mModel->scale(glm::vec3(2 * modelScale, 1 * modelScale, 1 * modelScale));

	// Lighting
	renderSystem->setAmbientColour(Colour::Grey25);
	renderSystem->setLightCount(1);
	renderSystem->setLight1Colour(Colour::White);

	// Pipelines
	auto pipeline = renderSystem->createRenderPipeline(getRenderPipelineName());
}

void ModelScene::createControls(mpp::RenderSystem* renderSystem)
{
	auto height = renderSystem->getWindowHeight();

	// Pane trefoil distance
	auto control = new Control("Pane Foil spread", Control::Orientation::Horizontal,
		[this]()
	{
		return Vector2(this->mTrefoilWindow->getPaneTrefoil().distance, 0.0f);
	},
		[this](Vector2 const& value)
	{
		this->mTrefoilWindow->getPaneTrefoil().distance = value.x;
	},
		[this](TrefoilWindow const* window)
	{
		return Vector2(8 - 320, (float)this->mWindowHeight - 100);
	},
		[]()
	{
		return Vector2(0.0f, 0.0f);
	},
		[this]()
	{
		return Vector2(this->mTrefoilWindow->getPaneTrefoil().radius * 2, 0.0f);
	}, height);

	control->showName(true);
	control->setColour(0.3f, 0.5f, 0.8f);
	mControls.push_back(control);
	
	// Pane trefoil radius
	control = new Control("Pane Foil Radius", Control::Orientation::Horizontal,
		[this]()
	{
		return Vector2(this->mTrefoilWindow->getPaneTrefoil().radius, 0.0f);
	},
		[this](Vector2 const& value)
	{
		this->mTrefoilWindow->getPaneTrefoil().radius = value.x;
	},
		[this](TrefoilWindow const* window)
	{
		return Vector2(8 - 320, (float)this->mWindowHeight - 132);
	},
		[this]()
	{
		return Vector2(this->mTrefoilWindow->getPaneTrefoil().distance * 0.5f, 0.0f);
	},
		[]()
	{
		return Vector2(1000.0f, 0.0f);
	}, height);

	control->showName(true);
	control->setColour(0.3f, 0.5f, 0.8f);
	mControls.push_back(control);

	// Upper trefoil distance
	control = new Control("Upper Foil spread", Control::Orientation::Horizontal,
		[this]()
	{
		return Vector2(this->mTrefoilWindow->getUpperTrefoil().distance, 0.0f);
	},
		[this](Vector2 const& value)
	{
		this->mTrefoilWindow->getUpperTrefoil().distance = value.x;
	},
		[this](TrefoilWindow const* window)
	{
		return Vector2(8 - 320, (float)this->mWindowHeight - 164);
	},
		[]()
	{
		return Vector2(0.0f, 0.0f);
	},
		[this]()
	{
		return Vector2(1000.0f, 0.0f); // return Vector2(this->mTrefoilWindow->getUpperTrefoil().radius, 0.0f);
	}, height);

	control->showName(true);
	control->setColour(0.3f, 0.5f, 0.8f);
	mControls.push_back(control);

	// Upper trefoil radius
	control = new Control("Upper Foil Radius", Control::Orientation::Horizontal,
		[this]()
	{
		return Vector2(this->mTrefoilWindow->getUpperTrefoil().radius, 0.0f);
	},
		[this](Vector2 const& value)
	{
		this->mTrefoilWindow->getUpperTrefoil().radius = value.x;
	},
		[this](TrefoilWindow const* window)
	{
		return Vector2(8 - 320, (float)this->mWindowHeight - 196);
	},
		[this]()
	{
		return Vector2(0.0f, 0.0f); // return Vector2(this->mTrefoilWindow->getUpperTrefoil().distance, 0.0f);
	},
		[]()
	{
		return Vector2(1000.0f, 0.0f);
	}, height);

	control->showName(true);
	control->setColour(0.3f, 0.5f, 0.8f);
	mControls.push_back(control);

	// Pane border size
	control = new Control("Pane Border", Control::Orientation::Horizontal,
		[this]()
	{
		return Vector2(this->mTrefoilWindow->getPaneSideOffset(), 0.0f);
	},
		[this](Vector2 const& value)
	{
		this->mTrefoilWindow->setPaneSideOffset(value.x);
	},
		[](TrefoilWindow const* window)
	{
		auto numPanes = window->getNumPanes();
		float xPos = -(window->getWidth() / 2.0f) +
			window->getPaneSideOffset() +
			numPanes * window->getPaneWidth() +
			(numPanes - 1) * window->getPaneSpacing();

		return Vector2(xPos, -10);
	},
		[]()
	{
		return Vector2(0.0f, 0.0f);
	},
		[]()
	{
		return Vector2(1000.0f, 0.0f);
	}, height);

	control->setColour(0.8f, 0.3f, 0.5f);
	mControls.push_back(control);

	// Border peak
	control = new Control("Peak", Control::Orientation::Vertical,
		[this]()
	{
		return Vector2(this->mTrefoilWindow->getBorderPeak(), 0.0f);
	},
		[this](Vector2 const& value)
	{
		this->mTrefoilWindow->setBorderPeak(value.x);
	},
		[](TrefoilWindow const* window)
	{
		float xPos = 580 - 320;
		return Vector2(xPos, window->getBorderPeakBase());
	},
		[]()
	{
		return Vector2(-1000.0f, 0.0f);
	},
		[]()
	{
		return Vector2(1000.0f, 0.0f);
	}, height);

	control->showValue(true);
	control->setColour(0.5f, 0.5f, 0.5f);
	mControls.push_back(control);

	// Shoulder offset
	control = new Control("Shoulder", Control::Orientation::Vertical,
		[this]()
	{
		return Vector2(this->mTrefoilWindow->getBorderShoulderOffset(), 0.0f);
	},
		[this](Vector2 const& value)
	{
		this->mTrefoilWindow->setBorderShoulderOffset(value.x);
	},
		[](TrefoilWindow const* window)
	{
		float xPos = 600 - 320;
		return Vector2(xPos, window->getPaneBaseOffset() + window->getPaneHeight());
	},
		[]()
	{
		return Vector2(-1000.0f, 0.0f);
	},
		[]()
	{
		return Vector2(1000.0f, 0.0f);
	}, height);

	control->setColour(0.5f, 0.5f, 0.5f);
	mControls.push_back(control);

	// Pane to upper trefoil gap
	control = new Control("Trefoil Gap", Control::Orientation::Vertical,
		[this]()
	{
		return Vector2(this->mTrefoilWindow->getPaneUpperBufferHeight(), 0.0f);
	},
		[this](Vector2 const& value)
	{
		this->mTrefoilWindow->setPaneUpperBufferHeight(value.x);
	},
		[](TrefoilWindow const* window)
	{
		float xPos = 580 - 320;
		return Vector2(xPos, window->getPaneBaseOffset() + window->getPaneHeight());
	},
		[]()
	{
		return Vector2(-1000.0f, 0.0f);
	},
		[]()
	{
		return Vector2(1000.0f, 0.0f);
	}, height);

	control->showValue(true);
	control->setColour(0.5f, 0.5f, 0.5f);
	control->setLabelOffset(32);
	mControls.push_back(control);

	// Pane to pane trefoil offset
	control = new Control("Trefoil Offset", Control::Orientation::Vertical,
		[this]()
	{
		return Vector2(this->mTrefoilWindow->getPaneTrefoilOffset(), 0.0f);
	},
		[this](Vector2 const& value)
	{
		this->mTrefoilWindow->setPaneTrefoilOffset(value.x);
	},
		[](TrefoilWindow const* window)
	{
		float xPos = 560 - 320;
		return Vector2(xPos, window->getPaneBaseOffset() + window->getPaneHeight());
	},
		[]()
	{
		return Vector2(-1000.0f, 0.0f);
	},
		[]()
	{
		return Vector2(1000.0f, 0.0f);
	}, height);

	control->showValue(true);
	control->setColour(0.5f, 0.5f, 0.5f);
	control->setLabelOffset(16);
	mControls.push_back(control);

	// Pane height
	control = new Control("Pane Height", Control::Orientation::Vertical,
		[this]()
	{
		return Vector2(this->mTrefoilWindow->getPaneHeight(), 0.0f);
	},
		[this](Vector2 const& value)
	{
		this->mTrefoilWindow->setPaneHeight(value.x);
	},
		[](TrefoilWindow const* window)
	{
		float xPos = 580 - 320;
		return Vector2(xPos, window->getPaneBaseOffset());
	},
		[]()
	{
		return Vector2(-1000.0f, 0.0f);
	},
		[]()
	{
		return Vector2(1000.0f, 0.0f);
	}, height);

	control->showValue(true);
	control->setColour(0.5f, 0.5f, 0.5f);
	mControls.push_back(control);

	// Base height
	control = new Control("Base Height", Control::Orientation::Vertical,
		[this]()
	{
		return Vector2(this->mTrefoilWindow->getPaneBaseOffset(), 0.0f);
	},
		[this](Vector2 const& value)
	{
		this->mTrefoilWindow->setPaneBaseOffset(value.x);
	},
		[](TrefoilWindow const* window)
	{
		float xPos = 580 - 320;
		return Vector2(xPos, 0);
	},
		[]()
	{
		return Vector2(0.0f, 0.0f);
	},
		[]()
	{
		return Vector2(1000.0f, 0.0f);
	}, height);

	control->showValue(true);
	control->setColour(0.5f, 0.5f, 0.5f);
	mControls.push_back(control);

	// Pane divider
	control = new Control("Pane Gap", Control::Orientation::Horizontal,
		[this]()
	{
		return Vector2(this->mTrefoilWindow->getPaneSpacing(), 0.0f);
	},
		[this](Vector2 const& value)
	{
		this->mTrefoilWindow->setPaneSpacing(value.x);
	},
		[](TrefoilWindow const* window)
	{
		return Vector2(0, -10);
	},
		[]()
	{
		return Vector2(1.0f, 0.0f);
	},
		[]()
	{
		return Vector2(1000.0f, 0.0f);
	}, height);

	control->setColour(0.8f, 0.3f, 0.5f);
	mControls.push_back(control);

	// Pane width
	control = new Control("Pane Width", Control::Orientation::Horizontal,
		[this]()
	{
		return Vector2(this->mTrefoilWindow->getPaneWidth() / 2, 0.0f);
	},
		[this](Vector2 const& value)
	{
		this->mTrefoilWindow->setPaneWidth(value.x * 2);
	},
		[](TrefoilWindow const* window)
	{
		return Vector2(0, -20);
	},
		[]()
	{
		return Vector2(1.0f, 0.0f);
	},
		[]()
	{
		return Vector2(1000.0f, 0.0f);
	}, height);

	control->setColour(0.8f, 0.3f, 0.5f);
	mControls.push_back(control);

	// Arc control point 1
	control = new Control("Arc 1", Control::Orientation::Free,
		[this]()
	{
		return this->mTrefoilWindow->getControl(0);
	},
		[this](Vector2 const& value)
	{
		this->mTrefoilWindow->setControl(0, value);
	},
		[](TrefoilWindow const* window)
	{
		auto w = window->getWidth();
		auto h = window->getHeight();
		auto s = window->getShoulderHeight();

		return Vector2(w / 4, h - 10) + window->getControl(0);
	},
		[]()
	{
		return Vector2(-1000.0f, -1000.0f);
	},
		[]()
	{
		return Vector2(1000.0f, 1000.0f);
	}, height);

	control->setColour(1.0f, 1.0f, 1.0f);
	mControls.push_back(control);

	// Arc control point 2
	control = new Control("Arc 2", Control::Orientation::Free,
		[this]()
	{
		return this->mTrefoilWindow->getControl(1);
	},
		[this](Vector2 const& value)
	{
		this->mTrefoilWindow->setControl(1, value);
	},
		[](TrefoilWindow const* window)
	{
		auto w = window->getWidth();
		auto h = window->getHeight();
		auto s = window->getShoulderHeight();

		return Vector2(w / 2 - 10, s + (h - s) * 0.5f) + window->getControl(1);
	},
		[]()
	{
		return Vector2(-1000.0f, -1000.0f);
	},
		[]()
	{
		return Vector2(1000.0f, 1000.0f);
	}, height);

	control->setColour(1.0f, 1.0f, 1.0f);
	mControls.push_back(control);
}

mpp::CameraPtr ModelScene::createCamera(ProgramOptions const& options) const
{
	float aspectRatio = options.screenWidth / (float)options.screenHeight;

	auto camera = new helper::FreeCamera(glm::vec3(0, 150, 550), 0.0f, 0.0f, 0.0f, 45.0f, aspectRatio);
	camera->setClipDistances(0.1f, 1000.0f);

	return std::shared_ptr<mpp::Camera>(camera);
}

void ModelScene::injectInput(InputManager* inputMgr)
{
	if (inputMgr->keyPressed(Key_S))
	{
		mTrefoilWindow->save("window.settings");
	}
	if (inputMgr->keyPressed(Key_L))
	{
		mTrefoilWindow->load("window.settings");
		mLineDataProvider->setDirty();
		mControlLinesDataProvider->setDirty();
		mControlHandlesDataProvider->setDirty();
		mSchematicDataProvider->setDirty();
	}
	if (inputMgr->keyDown(Key_UpArrow))
	{
		mModelMove = 50;
	}
	if (inputMgr->keyDown(Key_DownArrow))
	{
		mModelMove = -50;
	}
	if (inputMgr->keyDown(Key_LeftArrow))
	{
		mModelRotation = 1.0f;
	}
	if (inputMgr->keyDown(Key_RightArrow))
	{
		mModelRotation = -1.0f;
	}

	int x, y;
	inputMgr->getMousePosition(&x, &y);

	Vector2 viewOffset(mControlOffset.x, mWindowHeight - mControlOffset.y);

	mHoveredControl = nullptr;
	for (auto control: mControls)
	{
		if (control->isHovered(viewOffset, Vector2((float)x, (float)y)))
		{
			mHoveredControl = control;

			if (inputMgr->buttonPressed(Mouse_Left))
			{
				mSelectedControl = control;
			}

			break;
		}
	}

	if (inputMgr->buttonReleased(Mouse_Left))
	{
		mSelectedControl = nullptr;
	}

	if (mSelectedControl)
	{
		Vector2 move((float)(x - mOldMouseX), (float)(mOldMouseY - y));
		mSelectedControl->update(move);

		mLineDataProvider->setDirty();
		mControlLinesDataProvider->setDirty();
		mControlHandlesDataProvider->setDirty();
		mSchematicDataProvider->setDirty();
	}

	mOldMouseX = x;
	mOldMouseY = y;
}

void ModelScene::update(mpp::RenderSystem* renderSystem, float frameTime)
{
	mTotalTime += frameTime;

	Vector2 viewOffset(mControlOffset.x, mControlOffset.y);
	for (auto control: mControls)
	{
		control->setPosition(mTrefoilWindow);
	}

	// Update control lines
	mControlLinesDataProvider->update(frameTime);
	mControlsLineRenderer->update(mControlLinesDataProvider->getNumPrimitives());

	// Update control handles
	//mControlHandlesCircleRenderer->update(frameTime);

	mControlHandlesDataProvider->update(frameTime);
	mControlHandlesRenderer->update(mControlHandlesDataProvider->getNumPrimitives());

	// Update window schematic lines
	mLineDataProvider->update(frameTime);
	mLineRenderer->update(mLineDataProvider->getNumPrimitives());

	// Update 3d model
	mSchematicDataProvider->update(frameTime);
	mModelRenderer->update(mSchematicDataProvider->getNumPrimitives());

	static_cast<mpp::helper::FreeCamera*>(getCamera().get())->forward(mModelMove * frameTime);
	mModel->rotateSelf(mModelRotation * frameTime, glm::vec3(0, 1, 0));

	// Reset input vars
	mModelRotation = 0;
	mModelMove = 0;
}

void ModelScene::render(mpp::RenderSystem* renderSystem, World const& world, RenderOptions const& options)
{
	renderSystem->renderScene(getScene(), getCamera(), glm::vec2(0.0f, 0.0f), "Default");
	
	renderSystem->setProjection2dOrthographic();
	renderSystem->translateTransform2d(glm::vec2(renderSystem->getWindowWidth() / 4, 80));

	auto h = renderSystem->getWindowHeight();

	// Render labels
	for (auto control: mControls)
	{
		if (control->getOrientation() == Control::Orientation::Free)
		{
			continue;
		}

		auto const& controlPos = control->getLabelPosition();
		renderSystem->renderText(control->getName(), controlPos.x, h - controlPos.y, mpp::Colour::White);
	}
	
}