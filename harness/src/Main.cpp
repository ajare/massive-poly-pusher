#include <iostream>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtx/rotate_vector.hpp>
#pragma warning(pop)

#include "utils/StringUtils.h"

// RenderSystem
#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/FileProgramStream.h>
#include <mpp/Program.h>
#include <mpp/TextureStream.h>
#include <mpp/Texture.h>
#include <mpp/MaterialStream.h>
#include <mpp/Material.h>
#include <mpp/Model.h>
#include <mpp/ProgrammaticModelStream.h>
#include <mpp/BoxModelStream.h>
#include <mpp/CylinderModelStream.h>
#include <mpp/SphereModelStream.h>
#include <mpp/ProgrammaticMaterialStream.h>
#include <mpp/MppModelStream.h>

#include <mpp/mesh/MeshSpecification.h>

#include <mpp/helper/FreeCamera.h>
#include <mpp/helper/OrbitCamera.h>

#include "ProgramOptions.h"
#include "Helper.h"
#include "Logger.h"
#include "Particle.h"

// Platform
#include "sdl/WindowSDL.h"
#include "sdl/TimerSDL.h"
#include "sdl/InputManagerSDL.h"

#include "renderdoc/renderdoc_app.h"

using namespace std;
using namespace mpp;

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

vector<Particle> gParticles;

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
	unhookRenderdoc();
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

		//
		// Camera setup
		//
		glm::vec3 cameraPos(0, 1000, 0);
		glm::vec3 cameraTarget(0, 200, 0);

		float cameraPitch = atan2(cameraTarget.z - cameraPos.z, cameraTarget.y - cameraPos.y);
		cameraPitch += (3.14159f / 2.0f);
		helper::FreeCamera camera(cameraPos, 315, 0, 0);
		//helper::OrbitCamera camera(cameraPos, cameraTarget, glm::vec3(0, 1, 0), 45.0f);
		camera.setClipDistances(0.1f, 1000.0f);

		//
		// Model setup
		//

		// Programs
		//FileProgramStream* programStream = new FileProgramStream(gOptions.resourceLocation + "test.vert", gOptions.resourceLocation + "test.frag");
		//gResourceManager->createResource<Program>("test", ResourceStreamPtr(programStream));

		FileProgramStream* programStream = new FileProgramStream(gOptions.resourceLocation + "city.vert", gOptions.resourceLocation + "city.frag");
		gResourceManager->createResource<Program>("city", ResourceStreamPtr(programStream));

		// Textures
		//TextureStream* imageStream = loadImage(gOptions.resourceLocation + "marble_texture4662.jpg", false);
		//gResourceManager->createResource<Texture>("marble_texture", ResourceStreamPtr(imageStream));

		// Materials
		//ProgrammaticMaterialStream* meshMaterialStream = new ProgrammaticMaterialStream();
		//meshMaterialStream->setProgram("test");
		//meshMaterialStream->setTexture("tex", "marble_texture");
		//gResourceManager->createResource<Material>("statue_material", ResourceStreamPtr(meshMaterialStream))->load();

		ProgrammaticMaterialStream* meshMaterialStream = new ProgrammaticMaterialStream();
		meshMaterialStream->setProgram("city");
		gResourceManager->createResource<Material>("None", ResourceStreamPtr(meshMaterialStream))->load();

		// Models
		//auto modelStream = new MppModelStream(gOptions.resourceLocation + "statue/statue.mppmodel");
		//auto statueModel = gResourceManager->createResource<Model>("Statue", ResourceStreamPtr(modelStream));
		//statueModel->load();

		auto modelStream = new MppModelStream(gOptions.resourceLocation + "city/city.mppmodel");
		auto cityModel = gResourceManager->createResource<Model>("City", ResourceStreamPtr(modelStream));
		cityModel->load();

		/*
		mesh::MeshSpecification cubeSpec(mesh::Primitive::Type::Triangles);

		mesh::VertexBufferAttributeLayout* attribLayout = cubeSpec.createVertexBufferAttributeLayout();
		attribLayout->createAttribute(mesh::Vertex::Component::Position3, mesh::Vertex::DataType::Float, false);
		attribLayout->createAttribute(mesh::Vertex::Component::Normal3, mesh::Vertex::DataType::Float, false);
		attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
		attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::Float, true);
		cubeSpec.setStorageType(mesh::VertexBufferStorageType::Static);
		cubeSpec.setIndexedVertices(true);
		
		// primitive count = 20 * 4 ^ (res - 1)
		auto cubeStream = new SphereModelStream(cubeSpec, "bubbles_material", 1, 5); // approx 5k tris

		auto cubeModel = gResourceManager->createResource<Model>("Model.Cube", ResourceStreamPtr(cubeStream));
		cubeModel->load();

		*/
		//
		// Main loop
		//
		float accum = 0.0f;
		const float updateFreq = 1.0f / 60.0f;

		float fpsTimer = 0.0f, fps = 0.0f;
		int frameCount = 0;

		bool isFullScreen = gOptions.fullScreen;

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

				if (gInputMgr->keyDown(Key_LeftArrow))
				{
					viewAngle += 2.0f;
				}
				if (gInputMgr->keyDown(Key_RightArrow))
				{
					viewAngle -= 2.0f;
				}
				/*
				if (gInputMgr->keyDown(Key_Q))
				{
					lightAngle -= 2.0f;
					if (lightAngle < 0.0f)
					{
						lightAngle += 360.0f;
					}
				}
				if (gInputMgr->keyDown(Key_W))
				{
					lightAngle += 2.0f;
					if (lightAngle > 360.0f)
					{
						lightAngle -= 360.0f;
					}
				}
				*/
				if (gInputMgr->keyDown(Key_A))
				{
					lightHeight += 10.0f;
					if (lightHeight > 750.0f)
					{
						lightHeight = 750.0f;
					}
				}
				if (gInputMgr->keyDown(Key_Z))
				{
					lightHeight -= 10.0f;
					if (lightHeight < 0.0f)
					{
						lightHeight = 0.0f;
					}
				}

				if (gInputMgr->keyDown(Key_W))
				{
					camera.up(1.0f);
				}
				if (gInputMgr->keyDown(Key_S))
				{
					camera.down(1.0f);
				}
				if (gInputMgr->keyDown(Key_A))
				{
					camera.left(1.0f);
				}
				if (gInputMgr->keyDown(Key_D))
				{
					camera.right(1.0f);
				}
				if (gInputMgr->keyDown(Key_Q))
				{
					camera.roll(-1.0f);
				}
				if (gInputMgr->keyDown(Key_E))
				{
					camera.roll(1.0f);
				}

				// Change video mode to fullscreen
				if (gInputMgr->keyPressed(Key_F1))
				{
					isFullScreen = !isFullScreen;
					gWindow->setFullscreen(isFullScreen);
				}
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

			gRenderSystem->resetTransform();

			// Set light position
			glm::vec3 lightPos(0, lightHeight, 400);
			lightPos = glm::rotate(lightPos, glm::radians(lightAngle), glm::vec3(0, 1, 0));

			mpp::UniformCollection statueUniforms;
			statueUniforms.setUniform("light", lightPos);

			gRenderSystem->rotateTransform3d(viewAngle, glm::vec3(0, 1, 0));
			//gRenderSystem->renderModelBatched((Model&)*statueModel, true, &statueUniforms);
			gRenderSystem->renderModelBatched((Model&)*cityModel, true, &statueUniforms);

			
			// Add post-process effects
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

			// Finish scene
			auto ri = gRenderSystem->finishScene();

			// Text
			vector<string> lines;
			lines.push_back("FPS: " + utils::StringUtils::toString(fps));
			lines.push_back("Primitives: " + utils::StringUtils::toString(ri.primitivesRendered));
			lines.push_back("Program switches: " + utils::StringUtils::toString(ri.programSwitches));
			lines.push_back("Texture switches: " + utils::StringUtils::toString(ri.textureSwitches));
			gRenderSystem->renderText(lines, 0, 0, Colour::White);

			/*
			auto particleInstance = gRenderSystem->renderModelBatched(particleModel);
			particleInstance->getMeshInstance("Particles")->setRenderCount(numParticles);
			*/

			gWindow->show();
		}
	}
	catch (exception const& e)
	{
		cout << e.what() << endl;
		gLogger->message(e.what());
		return 1;
	}

	return 0;
}
