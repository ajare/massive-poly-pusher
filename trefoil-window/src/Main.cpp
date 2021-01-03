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
#include <mpp/TextureAtlas.h>
#include <mpp/MaterialStream.h>
#include <mpp/Material.h>
#include <mpp/Model.h>
#include <mpp/ProgrammaticModelStream.h>
#include <mpp/BoxModelStream.h>
#include <mpp/CylinderModelStream.h>
#include <mpp/SphereModelStream.h>
#include <mpp/GridModelStream.h>
#include <mpp/ProgrammaticMaterialStream.h>
#include <mpp/MppModelStream.h>
#include <mpp/StaticLogger.h>

#include <mpp/mesh/VertexData.h>
#include <mpp/mesh/MeshSpecification.h>
#include <mpp/mesh/MppMeshException.h>

#include "ProgramOptions.h"
#include "Helper.h"
#include "Logger.h"
#include "World.h"
#include "RenderOptions.h"
#include "Camera.h"
#include "Scene.h"
#include "ModelScene.h"

// Platform
#include "sdl/WindowSDL.h"
#include "sdl/TimerSDL.h"
#include "sdl/InputManagerSDL.h"

#include "renderdoc/renderdoc_app.h"

using namespace std;
using namespace mpp;

//
// Global variables
//
ProgramOptions gOptions;

::Logger* gLogger = nullptr;
Window* gWindow = nullptr;
Timer* gTimer = nullptr;
InputManager* gInputMgr = nullptr;

RenderSystem* gRenderSystem = nullptr;
ResourceManager* gResourceManager = nullptr;

vector<::Scene*> gScenes;
World gWorld;
RenderOptions gRenderOptions;

//
// Renderdoc integration for detailed diagnostics
//
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

//
// App initialisation
//
void startup()
{
	gOptions = parseProgramOptions("TrefoilWindow.cfg");

	gLogger = new ::Logger();
	if (!gLogger->initialise("TrefoilWindow.log"))
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

	mpp::enable_static_log(MPP_RESOURCE_LOGFILE, true);

	gRenderSystem = new RenderSystem(gWindow->getWidth(), gWindow->getHeight());
	
	gResourceManager = new ResourceManager(gRenderSystem);
	gResourceManager->setImageLoadFunction(loadImage);

	gRenderSystem->createCoreResources(gResourceManager);

	gInputMgr = new InputManagerSDL();
	gTimer = new TimerSDL();

	// Set up scenes
	gScenes.push_back(new ModelScene(gResourceManager));

	for (auto scene: gScenes)
	{
		scene->setup(gRenderSystem, gOptions);
	}

	// Set up world
	gWorld.pointLights.push_back(glm::vec3(0, 750, 400));

	// Set default render options
	gRenderOptions.wireframe = false;
}

//
// App shutdown
//
void shutdown()
{
	// Delete scenes
	for (auto scene: gScenes)
	{
		delete scene;
	}

	gScenes.clear();

	// Delete systems
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
	gLogger = nullptr;
}

//
// Entry point
//
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	int exitCode{ 0 };

	try
	{
		startup();

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

			if (gInputMgr->keyReleased(Key_F2))
			{
				gRenderOptions.wireframe = !gRenderOptions.wireframe;
			}

			// Update current state
			while (accum >= updateFreq)
			{
				accum -= updateFreq;

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
				//updateFreeCamera(camera, gInputMgr, frameTime);

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
				for (auto scene: gScenes)
				{
					scene->update(gRenderSystem, frameTime);
				}
			}

			//
			// Render
			//
			gRenderSystem->startStatsCollection();

			// Set light positions
			for (auto& light: gWorld.pointLights)
			{
				light = glm::rotate(light, glm::radians(lightAngle), glm::vec3(0, 1, 0));
			}

			// Render scenes
			for (auto scene: gScenes)
			{
				if (scene->getRender())
				{
					scene->render(gRenderSystem, gWorld, gRenderOptions);
				}
			}

			// Finish scene
			auto ri = gRenderSystem->finishStatsCollection();

			gWindow->show();
		}
	}
	catch (mpp::MppException const& e)
	{
		gLogger->message(e.what());
		gLogger->message(" - thrown by " + e.getFunction());
		gLogger->message(" - thrown at " + e.getFile() + ":" + to_string(e.getLine()));
		exitCode = 1;
	}
	catch (mpp::mesh::MppMeshException const& e)
	{
		gLogger->message(e.what());
		gLogger->message(" - thrown by " + e.getFunction());
		gLogger->message(" - thrown at " + e.getFile() + ":" + to_string(e.getLine()));
		exitCode = 1;
	}
	catch (exception const& e)
	{
		gLogger->message(e.what());
		exitCode = 1;
	}

	shutdown();
	return exitCode;
}
