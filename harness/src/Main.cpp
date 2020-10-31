#include <iostream>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtx/rotate_vector.hpp>
#pragma warning(pop)

#include "utils/StringUtils.h"

// RenderSystem
#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/Program.h>
#include <mpp/TextureStream.h>
#include <mpp/Texture.h>
#include <mpp/TextureAtlasStream.h>
#include <mpp/TextureAtlas.h>
#include <mpp/MaterialStream.h>
#include <mpp/Material.h>
#include <mpp/Model.h>
#include <mpp/ProgrammaticModelStream.h>
#include <mpp/BoxModelStream.h>
#include <mpp/CylinderModelStream.h>
#include <mpp/SphereModelStream.h>
#include <mpp/GridModelStream.h>
#include <mpp/FileMaterialStream.h>
#include <mpp/ProgrammaticMaterialStream.h>
#include <mpp/MppModelStream.h>
#include <mpp/QuadBatch.h>

#include <mpp/mesh/VertexData.h>
#include <mpp/mesh/MeshSpecification.h>
#include <mpp/mesh/MppMeshException.h>

#include <mpp/helper/FreeCamera.h>
#include <mpp/helper/OrbitCamera.h>

#include "ProgramOptions.h"
#include "Helper.h"
#include "Logger.h"
#include "Batches.h"
#include "QuadBatchRenderer.h"
#include "LineBatchRenderer.h"
#include "TriangleBatchRenderer.h"

// Platform
#include "sdl/WindowSDL.h"
#include "sdl/TimerSDL.h"
#include "sdl/InputManagerSDL.h"

#include "renderdoc/renderdoc_app.h"

using namespace std;
using namespace mpp;

struct ModelTransform
{
	mpp::ResourcePtr model;
	glm::vec3 position;
	glm::vec3 scale;
};

enum class ModelId
{
	Cube,
	Sphere,
	Cylinder,
	Grid,
	Statue
};

ProgramOptions gOptions;

::Logger* gLogger = nullptr;
Window* gWindow = nullptr;
Timer* gTimer = nullptr;
InputManager* gInputMgr = nullptr;
RenderSystem* gRenderSystem = nullptr;
ResourceManager* gResourceManager = nullptr;

// Renderdoc
HINSTANCE gRenderdocProc = 0;
RENDERDOC_API_1_1_1* gRenderdocApi = nullptr;

typedef int(*renderDocEntryFunc)(RENDERDOC_Version, void**);

void hookRenderdoc()
{
	string filepath = "renderdoc.dll";
	gRenderdocProc = LoadLibrary(wstring(filepath.begin(), filepath.end()).c_str());

	if (!gRenderdocProc)
	{
		string errMsg = "Could not load '" + filepath + "'.";
		throw exception(errMsg.c_str());
	}

	renderDocEntryFunc getApi = (renderDocEntryFunc)GetProcAddress(gRenderdocProc, "RENDERDOC_GetAPI");

	if (getApi(eRENDERDOC_API_Version_1_1_1, (void**)&gRenderdocApi) != 1)
	{
		throw exception("Could not get Renderdoc API.");
	}

	assert(gRenderdocApi->StartFrameCapture != nullptr && gRenderdocApi->EndFrameCapture != nullptr);

	// Set capture to F11
	RENDERDOC_InputButton captureKeys[1] = { RENDERDOC_InputButton::eRENDERDOC_Key_F11 };
	gRenderdocApi->SetCaptureKeys(captureKeys, 1);

	// Turn off overlay
	gRenderdocApi->MaskOverlayBits(0, 0);
	
	// Unload crash handler
	//api->UnloadCrashHandler();

	// Set path for captures
	gRenderdocApi->SetLogFilePathTemplate("renderdoc/frame");
}

void unhookRenderdoc()
{
	FreeLibrary(gRenderdocProc);
	gRenderdocProc = 0;
	gRenderdocApi = nullptr;
}

void startup()
{
	gOptions = parseProgramOptions("Harness.cfg");

	gLogger = new ::Logger();
	if (!gLogger->initialise("Harness.log"))
		throw exception("Could not create logger!");

#ifdef _DEBUG
	//hookRenderdoc();
#endif

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		throw exception("Could not initialise SDL video!");

	// Get video modes, organised by aspect ratio.
	DisplayModeSet displayModes = getVideoModes(0);

	gWindow = new WindowSDL();
	gWindow->create(gOptions.screenWidth, gOptions.screenHeight, gOptions.fullScreen, gOptions.vSync);

	gRenderSystem = new RenderSystem(gWindow->getWidth(), gWindow->getHeight());
	gResourceManager = new ResourceManager(gRenderSystem);
	gRenderSystem->createCoreResources(gResourceManager);

	gInputMgr = new InputManagerSDL();
	gTimer = new TimerSDL();
}

void shutdown()
{
	delete gTimer;
	delete gInputMgr;

	delete gRenderSystem;
	delete gResourceManager;

	if (gWindow)
	{
		gWindow->destroy();
		delete gWindow;
	}

	SDL_Quit();

#ifdef DEBUG
	//unhookRenderdoc();
#endif

	delete gLogger;
}


int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	atexit(shutdown);

	try
	{
		startup();

		//
		// Load resources
		//
		string resLoc = gOptions.resourceLocation;

		/*
		Loading resources:
		0. Create a MeshSpecification if needed for program, or programmatic model
		   - mesh::MeshSpecification modelSpec(mesh::Primitive::Type::Triangles);
		   - mesh::VertexBufferAttributeLayout* attribLayout = modelSpec.createVertexBufferAttributeLayout();
		   - attribLayout->createAttribute(mesh::Vertex::Component::Position3, mesh::Vertex::DataType::Float, false);
		   - attribLayout->createAttribute(mesh::Vertex::Component::Normal3, mesh::Vertex::DataType::Float, false);
		   - attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
		   - attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::Float, true);
		   - modelSpec.setStorageType(mesh::VertexBufferStorageType::Static);
		   - modelSpec.setIndexedVertices(true);
		1. Create programs
		   - auto STREAM = new ProgramProgramStream(meshSpec, VERTEX_FILE, FRAGMENT_FILE);
		   - ResourceManager::createResource(NAME, ResourceStreamPtr(STREAM));
		2. Create textures
		   - auto STREAM = loadImage(IMAGE_FILE, false);
		   - ResourceManager::createResource(NAME, ResourceStreamPtr(STREAM));
		3. Create materials
		   - auto STREAM = new ProgrammaticMaterialStream();
			 - STREAM->setProgram(PROGRAM_RESOURCE_NAME);
			 - STREAM->setProgram(<2d | 3d>, MODEL_SPEC, PROGRAM_TAGS);
		   - STREAM->setTexture(SAMPLER_NAME, TEXTURE_RESOURCE_NAME);
		   - ResourceManager::createResource(MATERIAL_NAME, ResourceStreamPtr(STREAM));
		   or:
		  - FileDataStream FILE_STREAM(MATERIAL_FILE);
		  - auto STREAM = new FileMaterialStream(FILE_STREAM);
		  - ResourceManager::createResource(MATERIAL_NAME, ResourceStreamPtr(STREAM));
		4. Create models
		   - auto STREAM = new BoxModelStream(MODEL_SPEC, MATERIAL_RESOURCE_NAME, ...);
		   - auto MODEL = ResourceManager::createResource(MODEL_NAME, ResourceStreamPtr(STREAM));
		5. Load model
		   - MODEL->load();
		*/

		//
		// Model setup
		//
		mesh::MeshSpecification modelSpec(mesh::Primitive::Type::Triangles);

		mesh::VertexBufferAttributeLayout* attribLayout = modelSpec.createVertexBufferAttributeLayout(false);
		attribLayout->createAttribute(mesh::Vertex::Component::Position3, mesh::Vertex::DataType::Float, false);
		attribLayout->createAttribute(mesh::Vertex::Component::Normal3, mesh::Vertex::DataType::Float, false);
		attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);

		//attribLayout = modelSpec.createVertexBufferAttributeLayout();
		attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::UnsignedByte, true);

		modelSpec.setStorageType(mesh::VertexBufferStorageType::Static);
		modelSpec.setIndexedVertices(true);

		//
		// Programs
		//

		//
		// Textures
		//

		// Marble
		TextureStream* textureStream = loadImage(gOptions.resourceLocation + "marble_texture4662.jpg", false);
		gResourceManager->createResource("marble_texture4662.jpg", ResourceStreamPtr(textureStream));

		// Bullets
		textureStream = loadImage(gOptions.resourceLocation + "bullet1.png", false);
		gResourceManager->createResource("bullet1.png", ResourceStreamPtr(textureStream));

		TextureAtlasStream* atlasStream = static_cast<TextureAtlasStream*>(loadImageAtlas(gOptions.resourceLocation + "bullets.png", false, 8, 1));
		gResourceManager->createResource("bullets.png", ResourceStreamPtr(atlasStream));

		// RGBA test
		textureStream = loadImage(gOptions.resourceLocation + "rgba.png", false);
		gResourceManager->createResource("rgba.png", ResourceStreamPtr(textureStream));

		// Arrow
		textureStream = loadImage(gOptions.resourceLocation + "arrow.png", false);
		gResourceManager->createResource("arrow.png", ResourceStreamPtr(textureStream));

		//
		// Materials
		//

		// Marble
		auto meshMaterialStream = new ProgrammaticMaterialStream(gResourceManager);

		meshMaterialStream->setProgram(false, modelSpec, {});

		meshMaterialStream->setTexture("TEX1", "marble_texture4662.jpg");
		gResourceManager->createResource("Material.Marble", ResourceStreamPtr(meshMaterialStream))->load();

		//
		// Models
		//

		// Cube
		/*
		auto cubeStream = new BoxModelStream(modelSpec, "Material.Marble", 1, 1, 1);

		auto cubeModel = gResourceManager->createResource("Model.Cube", ResourceStreamPtr(cubeStream));
		cubeModel->load();

		// Sphere
		auto sphereStream = new SphereModelStream(modelSpec, "Material.Marble", 15.0f, 1);

		auto sphereModel = gResourceManager->createResource("Model.Sphere", ResourceStreamPtr(sphereStream));
		sphereModel->load();
		
		// Cylinder
		auto cylinderStream = new CylinderModelStream(modelSpec, "Material.Marble", 30.0f, 15.0f, 10.0f, 72);

		auto cylinderModel = gResourceManager->createResource("Model.Cylinder", ResourceStreamPtr(cylinderStream));
		cylinderModel->load();
		*/

		// Grid
		auto gridStream = new GridModelStream(gResourceManager, modelSpec, "Material.Marble", 256, 256, 8, 8);

		auto gridModel = gResourceManager->createResource("Model.Grid", ResourceStreamPtr(gridStream));
		gridModel->load();

		// Quad
		mesh::MeshSpecification quadSpec(mesh::Primitive::Type::Triangles);

		attribLayout = quadSpec.createVertexBufferAttributeLayout(false);
		attribLayout->createAttribute(mesh::Vertex::Component::Position3, mesh::Vertex::DataType::HalfFloat, false);
		attribLayout->createAttribute(mesh::Vertex::Component::Normal3, mesh::Vertex::DataType::HalfFloat, false);
		attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::HalfFloat, false);
		attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::UnsignedByte, true);

		quadSpec.setStorageType(mesh::VertexBufferStorageType::Static);
		quadSpec.setIndexedVertices(true);
		
		auto quadStream = new ProgrammaticModelStream(gResourceManager);
		auto meshId = quadStream->createMesh("QuadTest", quadSpec, "Material.Marble", 16);
		
		mesh::VertexData quadVertexData(quadSpec, 4);
		
		quadVertexData.f16(0.0f, 0.0f, 0.0f).f16(0.0f, 1.0f, 0.0f).f16(0.0f, 0.0f).u8(255, 255, 255, 255);
		quadVertexData.f16(256.0f, 0.0f, 0.0f).f16(0.0f, 1.0f, 0.0f).f16(1.0f, 0.0f).u8(255, 255, 255, 255);
		quadVertexData.f16(256.0f, 0.0f, 256.0f).f16(0.0f, 1.0f, 0.0f).f16(1.0f, 1.0f).u8(255, 255, 255, 255);
		quadVertexData.f16(0.0f, 0.0f, 256.0f).f16(0.0f, 1.0f, 0.0f).f16(0.0f, 1.0f).u8(255, 255, 255, 255);
		quadStream->addVertexData(meshId, quadVertexData);
		
		quadStream->addTriangle(meshId, 0, 1, 2);
		quadStream->addTriangle(meshId, 2, 3, 0);

		auto quadModel = gResourceManager->createResource("Model.Quad", ResourceStreamPtr(quadStream));
		quadModel->load();

		// Statue
		/*
		auto statueStream = new MppModelStream(gOptions.resourceLocation + "statue/statue.mppmodel");

		auto statueModel = gResourceManager->createResource("Model.Statue", ResourceStreamPtr(statueStream));
		statueModel->load();
		*/

		//
		// Model transforms
		//
		vector< ModelTransform> modelTranforms =
		{
			//{ cubeModel, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(150.0f, 150.0f, 150.0f)},
			//{ sphereModel, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(10.0f, 10.0f, 10.0f)},
			//{ cylinderModel, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(10.0f, 10.0f, 10.0f)},
			{ gridModel, glm::vec3(0.0f, -100.0f, 50.0f), glm::vec3(1.0f, 1.0f, 1.0f)},
			{ quadModel, glm::vec3(-128.0f, -100.0f, -178.0f), glm::vec3(1.0f, 1.0f, 1.0f)}
			//{ statueModel, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f)}
		};

		auto currentModelId = 1;

		//
		// 2d batch objects
		//
		size_t lineBatchCount{ 4 };
		size_t triangleBatchCount{ 32 };
		size_t quadBatchCount{ 4 };

		/*
		To have a quad batch which uses a rendertexture (eg for a custom image), do the following:
		- Create a subclass of TextureRenderer, which may have various Renderers, eg TriangleBatchRenderer
		  which will take a DataProvider.
		  
		  Initialisation:
		  - Instantiate TextureRenderer subclass
		  - Instantiate QuadBatchDataProvider subclass
		  - Instantiate QuadBatchRenderer, passing in the two classes

		  Update:
		  - Update the time on the TextureRenderer if you want an animated texture
		  - Set the number of quads on the QuadBatchDataProvider
		  - Update the time on the QuadBatchDataProvider
		  - Update the QuadBatchRenderer

		  Render:
		    - TextureRenderer::updateRenderTexture(QuadBatchRenderer::getBatch()::getTexture()) if you want
			  an animated texture
			- QuadBatchRenderer::render()
		*/
		auto circleRenderer = make_shared<CircleRenderer>("Circles", gRenderSystem, gResourceManager);
		
		QuadBatchRendererParams quadParams(
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
			circleRenderer
		);

		auto quadData = make_shared<TestQuadBatchDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>>(gRenderSystem, quadBatchCount);

		QuadBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat> 
			quadBatchRenderer("TestQuads", 
				quadParams,
				quadData,
				quadBatchCount,
				gRenderSystem, 
				gResourceManager);

		quadBatchRenderer.create();
		
		LineBatchRendererParams lineParams
		{
			true,
			true,
			false
		};

		auto lineData = make_shared<TestLineBatchDataProvider<mpp::mesh::DataTypeFloat>>(gRenderSystem, lineBatchCount);

		LineBatchRenderer<mpp::mesh::DataTypeFloat>
			lineBatchRenderer("TestLines",
				lineParams,
				lineData,
				lineBatchCount,
				gRenderSystem,
				gResourceManager);

		lineBatchRenderer.create();

		TriangleBatchRendererParams triangleParams
		{
			false,
			false,
			true,
		};

		auto triangleData = make_shared<TestTriangleBatchDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>>(gRenderSystem, triangleBatchCount);

		TriangleBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>
			triangleBatchRenderer("TestTriangles",
				triangleParams,
				triangleData,
				gResourceManager->getResource("bullets.png"),
				quadBatchCount,
				gRenderSystem,
				gResourceManager);

		triangleBatchRenderer.create();

		//
		// Camera setup
		//
		helper::FreeCamera camera(glm::vec3(0, 0, 400), 0.0f, 0.0f, 0.0f);
		camera.setClipDistances(0.1f, 2000.0f);
		camera.setFov(45.0f);

		//
		// Main loop
		//
		float accum = 0.0f;
		const float updateFreq = 1.0f / 60.0f;

		float fpsTimer = 0.0f, fps = 0.0f;
		int frameCount = 0;

		bool isFullScreen = gOptions.fullScreen;
		bool wireframe = false;

		float viewAngle = 0.0f;
		float lightAngle = 0.0f, lightHeight = 750.0f;

		// Main loop
		gTimer->reset();
		bool running = true;
		float totalTime = 0.0f;
		while (running)
		{
			// Get frame time
			float frameTime = gTimer->getDeltaTime();
			accum += frameTime;
			totalTime += frameTime;

			// Calculate FPS
			frameCount++;
			fpsTimer += frameTime;
			if (fpsTimer >= 1.0f)
			{
				fpsTimer -= 1.0f;
				fps = (float)frameCount;
				frameCount = 0;
			}

			// Process window messages
			gWindow->processEvents(gInputMgr);
			gInputMgr->update();

			if (gInputMgr->keyPressed(Key_Escape))
			{
				running = false;
			}

			if (gInputMgr->keyPressed(Key_F9))
			{
				wireframe = !wireframe;
			}

			if (gInputMgr->keyPressed(Key_F10))
			{
				gRenderdocApi->TriggerCapture();
			}

			if (gInputMgr->keyPressed(Key_O))
			{
				lineBatchCount++;
				triangleBatchCount++;
				quadBatchCount++;
			}
			if (gInputMgr->keyPressed(Key_P))
			{
				lineBatchCount--; if (lineBatchCount == ~0u) lineBatchCount = 0;
				triangleBatchCount--; if (triangleBatchCount == ~0u) triangleBatchCount = 0;
				quadBatchCount--; if (quadBatchCount == ~0u) quadBatchCount = 0;
			}

			// Update current state
			while (accum >= updateFreq)
			{
				accum -= updateFreq;

				float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
				float forwardBack = 0.0f, upDown = 0.0f, rightLeft = 0.0f;

				// Select model
				/*
				if (gInputMgr->keyPressed(Key_1))
				{
					currentModelId = ModelId::Cube;
				}
				else if (gInputMgr->keyPressed(Key_2))
				{
					currentModelId = ModelId::Sphere;
				}
				else if (gInputMgr->keyPressed(Key_3))
				{
					currentModelId = ModelId::Cylinder;
				}
				else if (gInputMgr->keyPressed(Key_4))
				{
					currentModelId = ModelId::Grid;
				}
				else if (gInputMgr->keyPressed(Key_5))
				{
					currentModelId = ModelId::Statue;
				}
				*/
				// Rotate model
				if (gInputMgr->keyDown(Key_LeftArrow))
				{
					viewAngle += 60.0f * frameTime;
				}
				if (gInputMgr->keyDown(Key_RightArrow))
				{
					viewAngle -= 60.0f * frameTime;
				}

				// Move camera
				if (gInputMgr->keyDown(Key_W))
				{
					camera.forward(50.0f * frameTime);
				}
				if (gInputMgr->keyDown(Key_S))
				{
					camera.backward(50.0f * frameTime);
				}
				if (gInputMgr->keyDown(Key_A))
				{
					camera.left(50.0f * frameTime);
				}
				if (gInputMgr->keyDown(Key_D))
				{
					camera.right(50.0f * frameTime);
				}
				if (gInputMgr->keyDown(Key_R))
				{
					camera.up(50.0f * frameTime);
				}
				if (gInputMgr->keyDown(Key_V))
				{
					camera.down(50.0f * frameTime);
				}
				if (gInputMgr->keyDown(Key_Q))
				{
					camera.roll(-60.0f * frameTime);
				}
				if (gInputMgr->keyDown(Key_E))
				{
					camera.roll(60.0f * frameTime);
				}

				// Light
				if (gInputMgr->keyDown(Key_T))
				{
					lightAngle -= 50 * frameTime;
				}
				if (gInputMgr->keyDown(Key_Y))
				{
					lightAngle += 50 * frameTime;
				}
				if (gInputMgr->keyDown(Key_G))
				{
					lightHeight += 60 * frameTime;
				}
				if (gInputMgr->keyDown(Key_B))
				{
					lightHeight -= 60 * frameTime;
				}

				// Change video mode to fullscreen
				if (gInputMgr->keyPressed(Key_F1))
				{
					isFullScreen = !isFullScreen;
					gWindow->setFullscreen(isFullScreen);
				}

				// Logic
				circleRenderer->update(frameTime);
				quadData->setNumQuads(quadBatchCount);
				quadData->update(frameTime);
				quadBatchCount = quadBatchRenderer.update(quadBatchCount);

				lineData->setNumLines(lineBatchCount);
				lineData->update(frameTime);
				lineBatchCount = lineBatchRenderer.update(lineData->getNumLines());

				triangleData->setNumTriangles(triangleBatchCount);
				triangleData->update(frameTime);
				triangleBatchCount = triangleBatchRenderer.update(triangleData->getNumTriangles());
			}

			// Render scene

			// Should have a Scene object which takes a render target, camera, post-fx.
			// Call render methods on the scene object, rather than the rendersystem

			// For glow, can we render to multiple targets?
			gRenderSystem->startScene();
			gRenderSystem->clearScreen(Colour::Grey50);

			gRenderSystem->setProjection3dPerspective(
				camera.getFov(),
				camera.getNearClipDistance(),
				camera.getFarClipDistance());

			auto cameraPos = camera.getPosition();
			auto cameraDir = camera.getDirection();
			auto cameraUp = camera.getUp();
			gRenderSystem->setCamera3d(cameraPos, cameraPos + cameraDir, cameraUp);

			// Set light position
			glm::vec3 lightPos(0, lightHeight, 400);
			lightPos = glm::rotate(lightPos, glm::radians(lightAngle), glm::vec3(0, 1, 0));

			//
			// Render model
			//
			gRenderSystem->resetTransform();
			gRenderSystem->translateTransform3d(modelTranforms[(int)currentModelId].position);
			gRenderSystem->scaleTransform3d(modelTranforms[(int)currentModelId].scale);
			gRenderSystem->rotateTransform3d(viewAngle, glm::vec3(0, 1, 0));

			auto mi = gRenderSystem->renderModelBatched((Model&)*modelTranforms[(int)currentModelId].model, true);
			mi->setWireframe(wireframe);

			//
			// Add post-process effects
			//
			/*
			UniformCollection filmGrainUniforms;
			filmGrainUniforms.setUniform("strength", 32.0f);
			filmGrainUniforms.setUniform("time", totalTime);
			gRenderSystem->addPostEffect("__mpp_mat_filmgrain", filmGrainUniforms, 0, BlendMode::Zero, BlendMode::SrcColour);
			*/

			// Bloom: 
			// 1. render quad to blur1 target using blur program (horizontal) and blur attachment of scene target as input texture.
			// 2. render quad to blur2 target using blur program (vertical) and blur1 target as input texture.
			// 3. Repeat steps 1 & 2 a number of times (though use blur2 target as input texture for step 1).
			// 4. Blend blur2 to screen

			// Check whether we need to use a float texture rather than uint8 for bloom.

			// Maybe just: gRenderSystem->enableBloom()

			/*
			UniformCollection vignetteUniforms;
			vignetteUniforms.setUniform("intensity", 15.0f);
			vignetteUniforms.setUniform("extent", 0.25f);
			gRenderSystem->addPostEffect("__mpp_mat_vignette", vignetteUniforms, 0, BlendMode::Zero, BlendMode::SrcColour);
			*/

			//
			// Batches
			//
			gRenderSystem->setProjection2dOrthographic();

			circleRenderer->updateRenderTexture(quadBatchRenderer.getBatch()->getTexture());

			quadBatchRenderer.render();
			//lineBatchRenderer.render();
			//triangleBatchRenderer.render();

			//lineBatchCount = updateLineBatch(gRenderSystem, lineBatch, lineBatchCount, totalTime);
			//triBatchCount = updateIndexedTriangleBatch(gRenderSystem, triBatch, triBatchCount, totalTime);

			// Finish scene
			auto ri = gRenderSystem->finishScene();

			// Text
			vector<string> lines;
			lines.push_back("FPS: " + utils::StringUtils::toString(fps));
			lines.push_back("Primitives: " + utils::StringUtils::toString(ri.primitivesRendered));
			lines.push_back("Program switches: " + utils::StringUtils::toString(ri.programSwitches));
			lines.push_back("Texture switches: " + utils::StringUtils::toString(ri.textureSwitches));
			lines.push_back("");
			lines.push_back("[1] Cube");
			lines.push_back("[2] Sphere");
			lines.push_back("[3] Cylinder");
			lines.push_back("[4] Grid");
			lines.push_back("[5] Statue");

			gRenderSystem->renderText(lines, 0, 0, Colour::White);

			gWindow->show();
		}

		//
		// Clean up
		//
	}
	catch (mpp::MppException const& e)
	{
		gLogger->message(e.what());
		gLogger->message(" - thrown by " + e.getFunction());
		gLogger->message(" - thrown at " + e.getFile() + ":" + to_string(e.getLine()));
		return 1;
	}
	catch (mpp::mesh::MppMeshException const& e)
	{
		gLogger->message(e.what());
		gLogger->message(" - thrown by " + e.getFunction());
		gLogger->message(" - thrown at " + e.getFile() + ":" + to_string(e.getLine()));
		return 1;
	}
	catch (exception const& e)
	{
		gLogger->message(e.what());
		return 1;
	}

	return 0;
}
