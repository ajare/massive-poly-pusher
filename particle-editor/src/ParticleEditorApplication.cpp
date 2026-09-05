#include "ParticleEditorApplication.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <mpp/BufferRenderer.h>
#include <mpp/Logger.h>
#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/app/FileDialog.h>
#include <mpp/app/ImGuiBackendData.h>
#include <mpp/app/ImGuiDataProvider.h>
#include <mpp/app/ImGuiPlatform.h>
#include <mpp/app/ImageLoader.h>
#include <mpp/app/InputManagerSDL.h>
#include <mpp/app/RenderSystemConfig.h>
#include <mpp/app/TimerSDL.h>
#include <mpp/app/WindowSDL.h>

#include "DiagnosticsView.h"
#include "ParticleDocument.h"
#include "ParticleInspector.h"
#include "ParticlePreview.h"

namespace particle_editor
{
	namespace
	{
		struct SdlLifetime
		{
			~SdlLifetime() { SDL_Quit(); }
		};

		std::string trim(std::string value)
		{
			auto first = value.find_first_not_of(" \t\r\n");
			if (first == std::string::npos) return {};
			return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
		}

		std::filesystem::path resourceRoot(std::filesystem::path const& iniPath)
		{
			std::ifstream input(iniPath);
			if (!input) throw std::runtime_error("Could not open Particle Editor configuration '" + iniPath.string() + "'.");
			std::string section;
			for (std::string line; std::getline(input, line);)
			{
				line = trim(line);
				if (line.empty() || line.front() == ';' || line.front() == '#') continue;
				if (line.front() == '[' && line.back() == ']')
				{
					section = trim(line.substr(1, line.size() - 2));
					continue;
				}
				auto separator = line.find('=');
				if (section == "Editor" && separator != std::string::npos &&
					trim(line.substr(0, separator)) == "resourcesLocation")
				{
					auto path = std::filesystem::path(trim(line.substr(separator + 1)));
					if (path.is_relative()) path = iniPath.parent_path() / path;
					path = std::filesystem::weakly_canonical(path);
					if (!std::filesystem::is_directory(path))
						throw std::runtime_error("Configured Particle Editor resource directory does not exist: " + path.string());
					return path;
				}
			}
			throw std::runtime_error("particle-editor.ini does not define [Editor] resourcesLocation.");
		}

		std::filesystem::path particlePath(std::filesystem::path path)
		{
			auto filename = path.filename().string();
			auto lower = filename;
			std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char value)
				{ return static_cast<char>(std::tolower(value)); });
			if (!lower.ends_with(".particle.yaml"))
			{
				if (path.extension() == ".yaml" || path.extension() == ".yml") path.replace_extension();
				path += ".particle.yaml";
			}
			return path;
		}

		void showFatal(std::string const& message)
		{
			std::fprintf(stderr, "Particle Editor fatal error: %s\n", message.c_str());
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Particle Editor Error", message.c_str(), nullptr);
		}
	}

	int ParticleEditorApplication::run(int argc, char** argv)
	{
		try
		{
			std::filesystem::path startupPath;
			for (int index = 1; index < argc; ++index)
			{
				std::string argument = argv[index];
				if (argument == "--help" || argument == "-h")
				{
					std::printf("ParticleEditor options:\n  --help, -h          Show this help.\n  --document-tests    Run document contract tests.\n  [effect.particle.yaml] Open a particle effect.\n");
					return 0;
				}
				if (argument == "--document-tests")
				{
					std::string failure;
					if (!runParticleDocumentTests(&failure))
					{
						std::fprintf(stderr, "Particle Editor document tests failed: %s\n", failure.c_str());
						return 1;
					}
					std::fprintf(stderr, "Particle Editor document tests passed.\n");
					return 0;
				}
				if (!argument.starts_with("--")) startupPath = argument;
			}

#if defined(__linux__)
			if (!SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "x11", SDL_HINT_OVERRIDE))
				throw std::runtime_error("Could not select SDL's X11 video driver for GLX.");
#endif
			if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
			SdlLifetime sdl;
			auto const* sdlBasePath = SDL_GetBasePath();
			if (!sdlBasePath || !*sdlBasePath) throw std::runtime_error("SDL could not determine the Particle Editor executable directory.");
			auto iniPath = std::filesystem::path(sdlBasePath) / "particle-editor.ini";
			auto resourcesPath = resourceRoot(iniPath);
			auto options = mpp::app::loadRenderSystemOptions(iniPath);

			ParticleDocument document;
			DiagnosticsView diagnostics;
			if (!startupPath.empty()) document.open(startupPath);
			diagnostics.setDocumentDiagnostics(document.diagnostics());

			WindowSDL window("Particle Editor");
			window.create(1280, 800, false, true);
			mpp::Logger logger;
			if (!logger.initialise("ParticleEditor.log", mpp::Logger::Level::Debug))
				throw std::runtime_error("Could not create Particle Editor log.");
			// ResourceManager must outlive RenderSystem because renderer-owned resources
			// release themselves back to it from RenderSystem's destructor.
			std::unique_ptr<mpp::ResourceManager> resourceManager;
			mpp::RenderSystem renderSystem(window.getWidth(), window.getHeight(), &logger, options);
			resourceManager = std::make_unique<mpp::ResourceManager>(&renderSystem, &logger);
			resourceManager->setImageLoadFunction(mpp::app::loadImageFile);
			renderSystem.createCoreResources(resourceManager.get());

			ImGuiBackendData backend{};
			imGuiSetup(&renderSystem, resourceManager.get(), &backend, true);
			auto font = resourceManager->getResource("__ImGui_Font__", true);
			auto provider = std::make_shared<ImGuiDataProvider>(std::vector<mpp::ResourcePtr>{ font });
			mpp::BufferRenderer uiRenderer(provider);
			renderSystem.getOrCreateRenderPipeline("ParticleEditor.UI");

			ParticlePreview preview(&renderSystem, resourceManager.get());
			preview.initialise(resourcesPath, 900, 650);
			std::string previewFailure;
			preview.install(document.specification(), &previewFailure);
			diagnostics.setPreviewFailure(previewFailure);
			auto previewTexture = provider->registerTexture(preview.texture());
			ParticleInspector inspector;
			mpp::app::AsyncParticleFileDialog fileDialog;
			enum class DialogPurpose { None, Open, Save };
			DialogPurpose dialogPurpose = DialogPurpose::None;
			bool showDiagnostics = false;
			bool resetLayout = true;
			bool showAbout = false;
			bool running = true;
			InputManagerSDL input;
			TimerSDL timer;
			timer.reset();
			float fps = 0.0f;

			auto installDocument = [&]
			{
				diagnostics.setDocumentDiagnostics(document.diagnostics());
				previewFailure.clear();
				preview.install(document.specification(), &previewFailure);
				diagnostics.setPreviewFailure(previewFailure);
			};
			auto saveTo = [&](std::filesystem::path const& path)
			{
				try
				{
					document.save(particlePath(path));
					diagnostics.setOperationFailure({});
					diagnostics.setDocumentDiagnostics(document.diagnostics());
				}
				catch (std::exception const& error) { diagnostics.setOperationFailure(error.what()); showDiagnostics = true; }
			};

			while (running)
			{
				float delta = timer.getDeltaTime();
				if (delta > 0.0f) fps = fps == 0.0f ? 1.0f / delta : fps * 0.9f + (1.0f / delta) * 0.1f;
				bool closeRequested = !window.processEvents(&input);
				imGuiHandleInput(&input, &backend);
				input.update();
				if (closeRequested || input.keyPressed(Key_Escape)) running = false;
				if (window.getWidth() > 0 && window.getHeight() > 0 &&
					(size_t(window.getWidth()) != renderSystem.getWindowWidth() ||
					 size_t(window.getHeight()) != renderSystem.getWindowHeight()))
					renderSystem.setDisplay(window.getWidth(), window.getHeight());

				if (auto result = fileDialog.poll())
				{
					if (!result->error.empty())
					{
						diagnostics.setOperationFailure(result->error);
						showDiagnostics = true;
					}
					else if (result->path)
					{
						if (dialogPurpose == DialogPurpose::Open)
						{
							if (document.open(*result->path))
							{
								diagnostics.setOperationFailure({});
								installDocument();
							}
							else
							{
								diagnostics.setDocumentDiagnostics(document.diagnostics());
								showDiagnostics = true;
							}
						}
						else if (dialogPurpose == DialogPurpose::Save) saveTo(*result->path);
					}
					dialogPurpose = DialogPurpose::None;
				}

				imGuiNewFrame(window.getWindow(), &backend);
				ImGui::NewFrame();
				bool requestNew = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N, ImGuiInputFlags_RouteGlobal);
				bool requestOpen = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal);
				bool requestSave = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal);
				bool requestSaveAs = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, ImGuiInputFlags_RouteGlobal);
				if (requestSaveAs) requestSave = false;

				if (ImGui::BeginMainMenuBar())
				{
					if (ImGui::BeginMenu("File"))
					{
						requestNew |= ImGui::MenuItem("New", "Ctrl+N");
						requestOpen |= ImGui::MenuItem("Open...", "Ctrl+O", false, !fileDialog.busy());
						requestSave |= ImGui::MenuItem("Save", "Ctrl+S");
						requestSaveAs |= ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, !fileDialog.busy());
						ImGui::Separator();
						if (ImGui::MenuItem("Exit")) running = false;
						ImGui::EndMenu();
					}
					if (ImGui::BeginMenu("View"))
					{
						ImGui::MenuItem("Diagnostics", nullptr, &showDiagnostics);
						if (ImGui::MenuItem("Reset Layout")) resetLayout = true;
						ImGui::EndMenu();
					}
					if (ImGui::BeginMenu("Particle Effect"))
					{
						if (ImGui::MenuItem("Rebuild Preview", "F5")) installDocument();
						ImGui::Separator();
						if (preview.isSimulationPaused())
						{
							if (ImGui::MenuItem("Resume Simulation")) preview.resumeSimulation();
						}
						else if (ImGui::MenuItem("Pause Simulation")) preview.pauseSimulation();
						if (ImGui::MenuItem("Step Simulation", nullptr, false, preview.ready())) preview.stepSimulation();
						ImGui::EndMenu();
					}
					if (ImGui::BeginMenu("Help"))
					{
						if (ImGui::MenuItem("About Particle Editor")) showAbout = true;
						ImGui::EndMenu();
					}
					ImGui::EndMainMenuBar();
				}
				if (ImGui::Shortcut(ImGuiKey_F5, ImGuiInputFlags_RouteGlobal)) installDocument();

				if (ImGui::BeginViewportSideBar("##ParticleEditorToolbar", ImGui::GetMainViewport(), ImGuiDir_Up,
					ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y * 2.0f,
					ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
				{
					if (ImGui::Button("New")) requestNew = true;
					ImGui::SameLine();
					if (ImGui::Button("Open")) requestOpen = true;
					ImGui::SameLine();
					if (ImGui::Button("Save")) requestSave = true;
					ImGui::SameLine();
					if (ImGui::Button("Rebuild Preview")) installDocument();
					ImGui::SameLine();
					if (ImGui::Button(preview.isSimulationPaused() ? "Resume" : "Pause"))
					{
						if (preview.isSimulationPaused()) preview.resumeSimulation();
						else preview.pauseSimulation();
					}
					ImGui::SameLine();
					if (ImGui::Button("Step")) preview.stepSimulation();
					ImGui::SameLine();
					ImGui::SetNextItemWidth(110.0f);
					float timeScale = preview.simulationTimeScale();
					if (ImGui::SliderFloat("Time scale", &timeScale, 0.0f, 4.0f, "%.2fx"))
						preview.setSimulationTimeScale(timeScale);
					ImGui::SameLine();
					ImGui::TextDisabled("PBR graph | %s", preview.ready() ? "Live" : "Unavailable");
				}
				ImGui::End();

				if (requestNew)
				{
					document.createNew();
					diagnostics.setOperationFailure({});
					installDocument();
				}
				if (requestOpen && !fileDialog.busy())
				{
					dialogPurpose = DialogPurpose::Open;
					fileDialog.open(window.getWindow());
				}
				if (requestSave && document.hasPath()) saveTo(document.path());
				else if ((requestSave || requestSaveAs) && !fileDialog.busy())
				{
					dialogPurpose = DialogPurpose::Save;
					auto suggested = document.hasPath() ? document.path() :
						(std::filesystem::current_path() / "Untitled Particle Effect.particle.yaml");
					fileDialog.save(window.getWindow(), suggested.string());
				}

				if (ImGui::BeginViewportSideBar("##ParticleEditorStatus", ImGui::GetMainViewport(), ImGuiDir_Down,
					ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y * 2.0f,
					ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
				{
					ImGui::Text("%s%s | %s | %.1f FPS", document.path().empty() ? "Unsaved" : document.path().string().c_str(),
						document.dirty() ? " *" : "", diagnostics.statusText().c_str(), fps);
					if (preview.stats().valid)
					{
						ImGui::SameLine();
						ImGui::TextDisabled("| %u particles | effects %u submitted / %u bounds-culled | GPU %.2f ms",
							preview.stats().activeParticles, preview.stats().submittedEffects,
							preview.stats().boundsCulledEffects, preview.stats().renderGpuMilliseconds);
					}
				}
				ImGui::End();

				ImGuiID dockspace = ImGui::GetID("ParticleEditor.Dockspace");
				ImGui::DockSpaceOverViewport(dockspace, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
				if (resetLayout || !ImGui::DockBuilderGetNode(dockspace) || !ImGui::DockBuilderGetNode(dockspace)->IsSplitNode())
				{
					resetLayout = false;
					ImGui::DockBuilderRemoveNode(dockspace);
					ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
					ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->WorkSize);
					ImGuiID left = 0, right = 0;
					ImGui::DockBuilderSplitNode(dockspace, ImGuiDir_Left, 0.30f, &left, &right);
					ImGui::DockBuilderDockWindow("Particle Effect", left);
					ImGui::DockBuilderDockWindow("Diagnostics", left);
					ImGui::DockBuilderDockWindow("MPP Viewport", right);
					ImGui::DockBuilderFinish(dockspace);
				}

				inspector.draw(document);
				if (showDiagnostics) diagnostics.draw(&showDiagnostics);
				ImGui::Begin("MPP Viewport");
				auto available = ImGui::GetContentRegionAvail();
				uint32_t viewportWidth = std::max(64u, uint32_t(std::max(0.0f, available.x)));
				uint32_t viewportHeight = std::max(64u, uint32_t(std::max(0.0f, available.y)));
				try
				{
					preview.resize(viewportWidth, viewportHeight);
				}
				catch (std::exception const& error)
				{
					diagnostics.setPreviewFailure("Preview resize failed: " + std::string(error.what()));
				}
				ImGui::Image(previewTexture, ImVec2(float(viewportWidth), float(viewportHeight)), ImVec2(0, 1), ImVec2(1, 0));
				ImGui::End();

				if (showAbout)
				{
					ImGui::OpenPopup("About Particle Editor");
					showAbout = false;
				}
				if (ImGui::BeginPopupModal("About Particle Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::TextUnformatted("Particle Editor\nCanonical version-2 particle effect authoring\nLive MPP PBR graph preview");
					if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
					ImGui::EndPopup();
				}

				SDL_SetWindowTitle(window.getWindow(), (document.displayName() + (document.dirty() ? " * - Particle Editor" : " - Particle Editor")).c_str());
				ImGui::Render();
				provider->setDrawData(ImGui::GetDrawData());
				renderSystem.startStatsCollection();
				preview.render();
				uiRenderer.render(&renderSystem);
				renderSystem.finishStatsCollection();
				window.show();
			}

			provider->unregisterTexture(previewTexture);
			preview.shutdown();
			imGuiShutdown(&backend);
			provider->clearRegisteredTextures();
			font.reset();
			if (resourceManager->getResource("__ImGui_Font__", true)) resourceManager->deleteResource("__ImGui_Font__");
			renderSystem.removeRenderPipeline("ParticleEditor.UI");
			renderSystem.destroyCoreResources();
			return 0;
		}
		catch (std::exception const& error)
		{
			showFatal(error.what());
			return 1;
		}
	}
}
