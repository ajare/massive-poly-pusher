#if defined(__SANITIZE_ADDRESS__)
// Redirect MemCheck's ASan reports to a log file instead of stderr, which is
// otherwise the only place they go and is easy to lose — DemoSuite is a
// WIN32 GUI app with no visible console.
extern "C" const char* __asan_default_options()
{
	return "log_path=DemoSuite.asan";
}
#endif

#include <format>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Shellapi.h>
#else
#include <dlfcn.h>
#endif

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtx/rotate_vector.hpp>
#pragma warning(pop)

#include "utils/StringUtils.h"

// RenderSystem
#include <mpp/ParticleGpuTests.h>
#include <mpp/RenderGraphGpuTests.h>
#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/StaticLogger.h>

#include <mpp/mesh/MppMeshException.h>

#include "mpp/app/ImGuiPlatform.h"
#include "mpp/app/ZipArchive.h"
#include "mpp/app/PackageManifest.h"
#include "mpp/app/RenderSystemConfig.h"
#include "ProgramOptions.h"
#include "Helper.h"
#include "Logger.h"
#include "World.h"
#include "RenderOptions.h"
#include "Camera.h"
#include "Scene.h"
#include "PackageScene.h"
#include "ParticleScene.h"

// Platform
#include "mpp/app/WindowSDL.h"
#include "mpp/app/TimerSDL.h"
#include "mpp/app/InputManagerSDL.h"

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
mpp::Logger* gMppLogger = nullptr;

ImGuiBackendData gImGuiBackendData;

std::filesystem::path executableDirectory()
{
#ifdef _WIN32
	std::vector<wchar_t> path(32768);
	auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
	if (length == 0 || length == path.size()) throw std::runtime_error("Could not determine the DemoSuite executable directory.");
	return std::filesystem::path(std::wstring(path.data(), length)).parent_path();
#elif defined(__linux__)
	std::error_code error;
	auto path = std::filesystem::read_symlink("/proc/self/exe", error);
	if (error || path.empty()) throw std::runtime_error("Could not determine the DemoSuite executable directory.");
	return path.parent_path();
#else
	return std::filesystem::current_path();
#endif
}

vector<::Scene*> gScenes;
World gWorld;
RenderOptions gRenderOptions;

void showFatalError(char const* text)
{
	fprintf(stderr,"DemoSuite fatal error: %s\n",text);
	fflush(stderr);
#ifdef _WIN32
	if (!GetConsoleWindow()) MessageBoxA(nullptr,text,"DemoSuite Error",MB_OK|MB_ICONERROR);
#endif
}

void showCommandLineHelp(char const* text)
{
#ifdef _WIN32
	if (!GetConsoleWindow()) AttachConsole(ATTACH_PARENT_PROCESS);
	HANDLE output = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	DWORD written = 0;
	if (output != INVALID_HANDLE_VALUE && WriteFile(output, text, (DWORD)strlen(text), &written, nullptr))
	{
		CloseHandle(output);
		return;
	}
	if (output != INVALID_HANDLE_VALUE) CloseHandle(output);
	MessageBoxA(nullptr, text, "DemoSuite command-line options", MB_OK | MB_ICONINFORMATION);
#else
	fputs(text, stdout);
	fflush(stdout);
#endif
}

bool gPackageSmokeTest{false};
bool gParticleTests{false};
bool gParticles{false};
std::filesystem::path gPackageDirectory;
std::filesystem::path gResourceRoot;
bool gStartupComplete{false};

//
// Renderdoc integration for detailed diagnostics
//
#ifdef _WIN32
using RenderdocModule = HMODULE;
#else
using RenderdocModule = void*;
#endif
RenderdocModule gRenderdocProc = nullptr;
RENDERDOC_API_1_1_1* gRenderdocApi = nullptr;

typedef int(*renderDocEntryFunc)(RENDERDOC_Version, void**);

void unhookRenderdoc();

void hookRenderdoc()
{
#ifdef _WIN32
	string filepath = "renderdoc.dll";
	gRenderdocProc = LoadLibraryW(std::filesystem::path(filepath).c_str());
	auto getApi = reinterpret_cast<renderDocEntryFunc>(gRenderdocProc ? GetProcAddress(gRenderdocProc, "RENDERDOC_GetAPI") : nullptr);
#else
	string filepath = "librenderdoc.so";
	gRenderdocProc = dlopen(filepath.c_str(), RTLD_NOW | RTLD_LOCAL);
	auto getApi = reinterpret_cast<renderDocEntryFunc>(gRenderdocProc ? dlsym(gRenderdocProc, "RENDERDOC_GetAPI") : nullptr);
#endif
	if (!gRenderdocProc) throw runtime_error("Could not load '" + filepath + "'.");
	if (!getApi || getApi(eRENDERDOC_API_Version_1_1_1, (void**)&gRenderdocApi) != 1)
	{
		unhookRenderdoc();
		throw runtime_error("Could not get RenderDoc API.");
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
	if (gRenderdocProc)
	{
#ifdef _WIN32
		FreeLibrary(gRenderdocProc);
#else
		dlclose(gRenderdocProc);
#endif
	}
	gRenderdocProc = nullptr;
	gRenderdocApi = nullptr;
}

//
// App initialisation
//
bool startup(int argc, char** argv)
{
	bool showHelp = false;
	std::filesystem::path packagePath;
	vector<std::filesystem::path> arguments;
#ifdef _WIN32
	int argumentCount = 0;
	auto wideArguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
	for (int index = 0; wideArguments && index < argumentCount; ++index) arguments.emplace_back(wideArguments[index]);
	if (wideArguments) LocalFree(wideArguments);
#else
	for (int index = 0; index < argc; ++index) arguments.emplace_back(argv[index]);
#endif

	for (size_t index = 1; index < arguments.size(); ++index)
	{
		if (arguments[index] == "--help" || arguments[index] == "-h")
		{
			showHelp = true;
			continue;
		}
		if (arguments[index] == "--package-smoke-test")
		{
			gPackageSmokeTest = true;
			continue;
		}
		if (arguments[index] == "--particle-tests")
		{
			gParticleTests = true;
			continue;
		}
		if (arguments[index] == "--particles")
		{
			gParticles = true;
			continue;
		}
		if (arguments[index] == "--package")
		{
			if (++index >= arguments.size()) throw runtime_error("--package requires a .mpppackage path.");
			packagePath = arguments[index];
			continue;
		}
	}

	if (showHelp)
	{
		showCommandLineHelp(
			"DemoSuite options:\r\n"
			"  --help, -h                              Show this help.\r\n"
			"  --package <file.mpppackage>             Load the packaged scene and pipeline.\r\n"
			"                                           Defaults to workspace.mpppackage next to the executable.\r\n"
			"  --package-smoke-test                    With --package, render 30 frames then exit.\r\n"
			"  --particles                              Run the standalone particle demo (no package required).\r\n"
			"  --particle-tests                         Run particle and render graph GPU tests, then exit.\r\n");
		return false;
	}

	if (!gParticleTests && !gParticles)
	{
		if (packagePath.empty()) packagePath = executableDirectory() / "workspace.mpppackage";
		gPackageDirectory = mpp::app::createUniqueTemporaryDirectory("MDS");
		mpp::app::ZipArchive::extract(packagePath, gPackageDirectory);
		mpp::app::readPackageManifest(gPackageDirectory / "manifest.xml");
	}

	auto const executableRoot = executableDirectory();
	auto const configurationPath = executableRoot / "DemoSuite.cfg";
	gOptions = parseProgramOptions(configurationPath.string());
	auto resourceRoot = std::filesystem::path(gOptions.resourceLocation);
	if (resourceRoot.is_relative()) resourceRoot = configurationPath.parent_path() / resourceRoot;
	resourceRoot = std::filesystem::weakly_canonical(resourceRoot);
	if (!std::filesystem::is_directory(resourceRoot))
		throw runtime_error("Configured DemoSuite resource directory does not exist: " + resourceRoot.string());
	gResourceRoot = resourceRoot;
	auto renderSystemOptions = mpp::app::loadRenderSystemOptions(executableRoot / "demosuite.ini");
	gLogger = new ::Logger();
	if (!gLogger->initialise("DemoSuite.log"))
		throw runtime_error("Could not create logger!");

	gMppLogger = new mpp::Logger();
	if (!gMppLogger->initialise("mpp.log", mpp::Logger::Level::Debug))
	{
		THROW_MPP("Could not initialise MPP logger", __LINE__, __FILE__, __func__);
	}

#ifdef _DEBUG
	//hookRenderdoc();
#endif

#if defined(__linux__)
	// The vendored GLEW build uses GLX. On Wayland SDL would otherwise create
	// an EGL context, which makes glewInit fail with "No GLX display".
	if (!SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "x11", SDL_HINT_OVERRIDE))
		throw runtime_error("Could not select SDL's X11 video driver for GLX.");
#endif
	if (!SDL_Init(SDL_INIT_VIDEO))
		throw runtime_error("Could not initialise SDL video: " + string(SDL_GetError()));
	gLogger->message("SDL video driver: " + string(SDL_GetCurrentVideoDriver()));

	// Get video modes, organised by aspect ratio.
	DisplayModeSet displayModes = getVideoModes(0);

	gWindow = new WindowSDL();
	gWindow->create(gOptions.screenWidth, gOptions.screenHeight, gOptions.fullScreen, gOptions.vSync);

	mpp::enable_static_log(MPP_RESOURCE_LOGFILE, true);

	gRenderSystem = new RenderSystem(gWindow->getWidth(), gWindow->getHeight(), gMppLogger, renderSystemOptions);
	
	gResourceManager = new ResourceManager(gRenderSystem, gMppLogger);
	gResourceManager->setImageLoadFunction(loadImage);

	gRenderSystem->createCoreResources(gResourceManager);

	imGuiSetup(gRenderSystem, gResourceManager, &gImGuiBackendData);

	gInputMgr = new InputManagerSDL();
	gTimer = new TimerSDL();

	if (gParticles && !gParticleTests)
		gScenes.push_back(new ParticleScene(gResourceManager, resourceRoot / "demo-suite" / "res"));
	else if (!gParticleTests)
		gScenes.push_back(new PackageScene(gResourceManager,gPackageDirectory));

	for (auto scene: gScenes)
	{
		scene->setup(gRenderSystem, gOptions);
	}

	// Set up world
	gWorld.pointLights.push_back(glm::vec3(0, 750, 400));

	// Set default render options
	gRenderOptions.wireframe = false;
	gStartupComplete = true;
	return true;
}

//
// App shutdown
//
void shutdown()
{
	// Delete scenes
	for (auto scene: gScenes)
	{
		scene->teardown();
		delete scene;
	}

	gScenes.clear();

	imGuiShutdown(&gImGuiBackendData);

	// Delete systems
	delete gTimer;
	delete gInputMgr;

	gRenderSystem->destroyCoreResources();
	delete gRenderSystem;

	gResourceManager->dumpResources("final-resources.csv");
	delete gResourceManager;

	delete gMppLogger;

	if (gWindow)
	{
		gWindow->destroy();
		delete gWindow;
	}

	SDL_Quit();
	if(!gPackageDirectory.empty())std::filesystem::remove_all(gPackageDirectory);

#ifdef DEBUG
	//unhookRenderdoc();
#endif

	delete gLogger;
	gLogger = nullptr;
}

//
// Entry point
//
#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// Preserve CLI automation modes in the GUI subsystem build. MSVC exposes
	// the parsed process arguments through these CRT globals.
	int argc = __argc;
	char** argv = __argv;
#else
int main(int argc, char** argv)
{
#endif
	int exitCode{ 0 };

	try
	{
		if(!startup(argc, argv))return 0;

		if (gParticleTests)
		{
			std::string suiteFailure;
			if (!mpp::runParticleGpuTests(gRenderSystem, &suiteFailure))
			{
				fprintf(stderr, "DemoSuite particle GPU tests failed: %s\n", suiteFailure.c_str());
				fflush(stderr);
				exitCode = 1;
			}
			else if (!mpp::runRenderGraphGpuTests(gRenderSystem, &suiteFailure))
			{
				fprintf(stderr, "DemoSuite render graph GPU tests failed: %s\n", suiteFailure.c_str());
				fflush(stderr);
				exitCode = 1;
			}
			else
			{
				bool serializedSmokePassed = false;
				try
				{
					ParticleScene serializedSmoke(gResourceManager, gResourceRoot / "demo-suite" / "res");
					serializedSmoke.setup(gRenderSystem, gOptions);
					for (int frame = 0; frame < 3; ++frame)
					{
						gRenderSystem->startStatsCollection();
						serializedSmoke.render(gRenderSystem, gWorld, gRenderOptions);
						serializedSmoke.present(gRenderSystem);
						gRenderSystem->renderToScreen();
						(void)gRenderSystem->finishStatsCollection();
					}
					serializedSmoke.teardown();
					serializedSmokePassed = true;
				}
				catch (std::exception const& error)
				{
					suiteFailure = "serialized particle effect smoke test failed: " + std::string(error.what());
				}
				if (!serializedSmokePassed)
				{
					fprintf(stderr, "DemoSuite particle GPU tests failed: %s\n", suiteFailure.c_str());
					fflush(stderr);
					exitCode = 1;
				}
				else
				{
					fprintf(stderr, "Particle and render graph GPU tests passed.\n");
					fflush(stderr);
				}
			}
			shutdown();
			return exitCode;
		}

		//
		// Main loop
		//
		float accum = 0.0f;
		const float updateFreq = 1.0f / 60.0f;

		float fpsTimer = 0.0f, fps = 0.0f;
		int frameCount = 0;

		bool isFullScreen = gOptions.fullScreen;

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
			if (!gWindow->processEvents(gInputMgr)) running = false;

			// Feed the raw SDL-derived events to ImGui before InputManager::update()
			// consumes them. Without this, sliders and combo boxes are rendered but
			// never receive mouse or keyboard interaction.
			imGuiHandleInput(gInputMgr, &gImGuiBackendData);

			gInputMgr->update();

			if (gInputMgr->keyPressed(Key_Escape))
			{
				running = false;
			}

			if (!gScenes.empty()) gScenes[0]->handleInput(gInputMgr);

			if (gInputMgr->keyReleased(Key_F2))
			{
				gRenderOptions.wireframe = !gRenderOptions.wireframe;
			}

			if (gRenderdocApi && gInputMgr->keyPressed(Key_F10))
			{
				gRenderdocApi->TriggerCapture();
			}

			// Update current state
			while (accum >= updateFreq)
			{
				accum -= updateFreq;

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

				imGuiNewFrame(static_cast<WindowSDL*>(gWindow)->getWindow(), &gImGuiBackendData);

				// Logic
				for (auto scene: gScenes)
				{
					scene->update(gRenderSystem, frameTime);
				}
			}

			//
			// Render
			//
			gRenderSystem->setDebugPreMessages({ std::format("FPS: {}", fps) });
			gRenderSystem->showDebugPanel(true);
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

			// Present the completed offscreen scene before drawing UI. Keeping text
			// out of the fullscreen presentation copy prevents its nearest-filtered
			// font atlas from being resampled by that pass.
			if (!gScenes.empty())
			{
				if (auto* packageScene = dynamic_cast<PackageScene*>(gScenes[0])) packageScene->present(gRenderSystem);
				else if (auto* particleScene = dynamic_cast<ParticleScene*>(gScenes[0])) particleScene->present(gRenderSystem);
			}
			gRenderSystem->renderToScreen();

			// Finish the 3D statistics and render all 2D overlays directly to the
			// backbuffer as the final operations of the frame.
			auto ri = gRenderSystem->finishStatsCollection();

			vector<string> lines;
			lines.push_back("F1: toggle fullscreen");
			lines.push_back("F2: toggle wireframe");
			lines.push_back("T/Y: light angle");
			lines.push_back("G/B: light height");
			if (!gScenes.empty())
			{
				auto sceneLines = gScenes[0]->getOverlayLines();
				lines.insert(lines.end(), sceneLines.begin(), sceneLines.end());
			}
			if (!gParticles) lines.push_back("Package camera: Alt+left orbit, Shift+Alt+left pan, Ctrl+Alt+left dolly");

			gRenderSystem->renderText(lines, 8, 0, Colour::White);

			gWindow->show();
			if(gPackageSmokeTest&&frameCount>=30)running=false;
		}
	}
	catch (mpp::MppException const& e)
	{
		if(gLogger){gLogger->message(e.what());gLogger->message(" - thrown by " + e.getFunction());gLogger->message(" - thrown at " + e.getFile() + ":" + to_string(e.getLine()));gLogger->message(" - stack trace: " + e.getStackTrace());}
		showFatalError(e.what());
		exitCode = 1;
	}
	catch (mpp::mesh::MppMeshException const& e)
	{
		if(gLogger){gLogger->message(e.what());gLogger->message(" - thrown by " + e.getFunction());gLogger->message(" - thrown at " + e.getFile() + ":" + to_string(e.getLine()));}
		showFatalError(e.what());
		exitCode = 1;
	}
	catch (exception const& e)
	{
		if(gLogger)gLogger->message(e.what());
		showFatalError(e.what());
		exitCode = 1;
	}

	if(gStartupComplete)shutdown();else if(!gPackageDirectory.empty())std::filesystem::remove_all(gPackageDirectory);
	return exitCode;
}
