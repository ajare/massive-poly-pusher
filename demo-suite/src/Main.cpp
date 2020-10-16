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

#include <mpp/mesh/VertexData.h>
#include <mpp/mesh/MeshSpecification.h>
#include <mpp/mesh/MppMeshException.h>

#include <mpp/helper/FreeCamera.h>
#include <mpp/helper/OrbitCamera.h>

#include "ProgramOptions.h"
#include "Helper.h"
#include "Logger.h"

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
	gOptions = parseProgramOptions("DemoSuite.cfg");

	gLogger = new ::Logger();
	if (!gLogger->initialise("DemoSuite.log"))
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
		2. Create textures
		   - auto STREAM = loadImage(IMAGE_FILE, false);
		   - ResourceManager::createResource<Texture>(NAME, ResourceStreamPtr(STREAM));
		3. Create materials
		   - auto STREAM = new ProgrammaticMaterialStream();
			 - STREAM->setProgram(PROGRAM_RESOURCE_NAME);
			 - STREAM->setProgram(<2d | 3d>, MODEL_SPEC, PROGRAM_TAGS);
		   - STREAM->setTexture(SAMPLER_NAME, TEXTURE_RESOURCE_NAME);
		   - ResourceManager::createResource<Material>(MATERIAL_NAME, ResourceStreamPtr(STREAM));
		   or:
		  - FileDataStream FILE_STREAM(MATERIAL_FILE);
		  - auto STREAM = new FileMaterialStream(FILE_STREAM);
		  - ResourceManager::createResource<Material>(MATERIAL_NAME, ResourceStreamPtr(STREAM));
		4. Create models
		   - auto STREAM = new BoxModelStream(MODEL_SPEC, MATERIAL_RESOURCE_NAME, ...);
		   - auto MODEL = ResourceManager::createResource<Model>(MODEL_NAME, ResourceStreamPtr(STREAM));
		5. Load model
		   - MODEL->load();
		*/

		//
		// MeshSpecifications
		//

		/*
		MeshSpecifications are composed of vertex attribute layouts.  Generally, you will use one layout, although
		using multiple is useful when you want to change certain attributes without having to reupload the ones that
		do not change.  For instance, if you are animating the positions (and normals) on the CPU, but want texcoords
		and colours to stay the same, it is more efficient to have one layout with position and normal, and one with
		texcoords and colour.
		*/
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

		/*
		Programs are composed of templated shaders.  Templating generates version, attrib and uniform
		information, along with conditionals that affect codepaths.

		You can call ResourceManager::getDefault[2d|3d]Program, passing in a MeshSpecification and flags.  This takes either
		a set of shaders ready to be templated, or uses the built-in ones.

		Or you can manually create a Program by loading in the shaders from disk, and then creating a Parser, setting the shader
		source and MeshSpecification.

		Programs, therefore, are specified with a MeshSpecification, and a list of shaders.
		*/

		//
		// Textures
		//
		
		/*
		Textures are image files which are loaded with a helper function into a TextureStream, which takes the raw loaded data.
		*/
		TextureStream* textureStream = loadImage(gOptions.resourceLocation + "marble_texture4662.jpg", false);
		gResourceManager->createResource<Texture>("marble_texture4662.jpg", ResourceStreamPtr(textureStream));

		//
		// Materials
		//

		/*
		Materials consist of a Program, and zero or more Textures.
		TODO: a Material should be able to consist of a set of shaders, and zero or more Textures.  That is to say, we can apply
		the same Material to multiple Models (ie with different MeshSpecifications).
		*/
		auto meshMaterialStream = new ProgrammaticMaterialStream(gResourceManager);
		meshMaterialStream->setProgram(false, modelSpec, {});
		meshMaterialStream->setTexture("TEX1", "marble_texture4662.jpg");
		gResourceManager->createResource<Material>("Material.Marble", ResourceStreamPtr(meshMaterialStream))->load();

		FileDataStream fileDataStream(gOptions.resourceLocation + "statue/statue.material");
		auto statueMaterialStream = new FileMaterialStream(gResourceManager, fileDataStream);
		gResourceManager->createResource<Material>("statue_material", ResourceStreamPtr(statueMaterialStream))->load();

		//
		// Models
		//
		auto statueStream = new MppModelStream(gResourceManager, gOptions.resourceLocation + "statue/statue.mppmodel");
		auto statueModel = gResourceManager->createResource<Model>("Model.Statue", ResourceStreamPtr(statueStream));
		statueModel->load();

		//
		// Model transforms
		//
		ModelTransform statueTransform
		{
			statueModel, 
			glm::vec3(0.0f, 0.0f, 0.0f), 
			glm::vec3(1.0f, 1.0f, 1.0f)
		};

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

			// Update current state
			while (accum >= updateFreq)
			{
				accum -= updateFreq;

				float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
				float forwardBack = 0.0f, upDown = 0.0f, rightLeft = 0.0f;

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
				// ...
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

			mpp::UniformCollection modelUniforms;
			modelUniforms.setUniform("light", lightPos);

			//
			// Render model
			//
			gRenderSystem->resetTransform();
			gRenderSystem->translateTransform3d(statueTransform.position);
			gRenderSystem->scaleTransform3d(statueTransform.scale);
			gRenderSystem->rotateTransform3d(viewAngle, glm::vec3(0, 1, 0));

			auto mi = gRenderSystem->renderModelBatched((Model&)*statueTransform.model, true, &modelUniforms);
			mi->setWireframe(wireframe);

			//
			// Add post-process effects
			//

			// Finish scene
			auto ri = gRenderSystem->finishScene();

			// Text
			vector<string> lines;
			lines.push_back("FPS: " + utils::StringUtils::toString(fps));
			lines.push_back("Primitives: " + utils::StringUtils::toString(ri.primitivesRendered));
			lines.push_back("Program switches: " + utils::StringUtils::toString(ri.programSwitches));
			lines.push_back("Texture switches: " + utils::StringUtils::toString(ri.textureSwitches));

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
