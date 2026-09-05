#include "ParticleEditorApplication.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <cmath>
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
#include <mpp/resource-parsers/ParticleEffectParser.h>

#include "DiagnosticsView.h"
#include "ParticleDocument.h"
#include "ParticleInspector.h"
#include "ParticlePreview.h"
#include "ParticlePreviewPreferences.h"
#include "ParticleResourceLibrary.h"

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

		struct EditorConfiguration
		{
			std::filesystem::path resourceRoot;
			std::filesystem::path resourceLibrary;
		};

		EditorConfiguration editorConfiguration(std::filesystem::path const& iniPath)
		{
			std::ifstream input(iniPath);
			if (!input) throw std::runtime_error("Could not open Particle Editor configuration '" + iniPath.string() + "'.");
			EditorConfiguration result;
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
				if (section != "Editor" || separator == std::string::npos) continue;
				auto key = trim(line.substr(0, separator));
				auto value = std::filesystem::path(trim(line.substr(separator + 1)));
				if (key == "resourcesLocation" || key == "resourceRoot") result.resourceRoot = std::move(value);
				else if (key == "resourceLibrary") result.resourceLibrary = std::move(value);
			}
			if (result.resourceRoot.empty())
				throw std::runtime_error("particle-editor.ini does not define [Editor] resourceRoot/resourcesLocation.");
			if (result.resourceRoot.is_relative()) result.resourceRoot = iniPath.parent_path() / result.resourceRoot;
			result.resourceRoot = std::filesystem::weakly_canonical(result.resourceRoot);
			if (!std::filesystem::is_directory(result.resourceRoot))
				throw std::runtime_error("Configured Particle Editor resource root does not exist: " + result.resourceRoot.string());
			return result;
		}

		void showFatal(std::string const& message)
		{
			std::fprintf(stderr, "Particle Editor fatal error: %s\n", message.c_str());
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Particle Editor Error", message.c_str(), nullptr);
		}

		int validateParticleEffect(std::filesystem::path const& path)
		{
			auto result = mpp::resource_parsers::ParticleEffectParser::fromFile(path.string());
			for (auto const& diagnostic : result.diagnostics.getDiagnostics())
			{
				auto const& location = diagnostic.location;
				auto source = location.document.empty() ? path.string() : location.document;
				if (location.line) source += ":" + std::to_string(location.line);
				if (location.column) source += ":" + std::to_string(location.column);
				std::fprintf(stderr, "%s: %s %s: %s\n", source.c_str(),
					mpp::diagnosticSeverityName(diagnostic.severity), diagnostic.code.c_str(), diagnostic.message.c_str());
			}
			if (result.diagnostics.hasErrors()) return 1;
			std::fprintf(stdout, "%s: valid particle effect\n", path.string().c_str());
			return 0;
		}
	}

	int ParticleEditorApplication::run(int argc, char** argv)
	{
		try
		{
			if (argc >= 2 && std::string(argv[1]) == "--validate")
			{
				if (argc != 3)
				{
					std::fprintf(stderr, "usage: ParticleEditor --validate <effect.particle.yaml>\n");
					return 2;
				}
				return validateParticleEffect(argv[2]);
			}

			std::filesystem::path startupPath;
			for (int index = 1; index < argc; ++index)
			{
				std::string argument = argv[index];
				if (argument == "--help" || argument == "-h")
				{
					std::printf("ParticleEditor options:\n  --help, -h                         Show this help.\n  --validate <effect.particle.yaml>  Validate with production diagnostics.\n  --document-tests                   Run document workflow tests.\n  --preview-tests                    Run editor preview preference tests.\n  --resource-tests                   Run editor resource-library tests.\n  [effect.particle.yaml]             Open a particle effect.\n");
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
				if (argument == "--preview-tests")
				{
					std::string failure;
					if (!runParticlePreviewPreferenceTests(&failure))
					{
						std::fprintf(stderr, "Particle Editor preview tests failed: %s\n", failure.c_str());
						return 1;
					}
					std::fprintf(stderr, "Particle Editor preview tests passed.\n");
					return 0;
				}
				if (argument == "--resource-tests")
				{
					std::string failure;
					if (!runParticleResourceLibraryTests(&failure))
					{
						std::fprintf(stderr, "Particle Editor resource tests failed: %s\n", failure.c_str());
						return 1;
					}
					std::fprintf(stderr, "Particle Editor resource tests passed.\n");
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
			auto editorConfig = editorConfiguration(iniPath);
			auto const& resourcesPath = editorConfig.resourceRoot;
			auto options = mpp::app::loadRenderSystemOptions(iniPath);
			char* preferenceRoot = SDL_GetPrefPath("ajare", "ParticleEditor");
			if (!preferenceRoot) throw std::runtime_error("SDL could not determine the Particle Editor preferences directory.");
			auto previewPreferencesPath = std::filesystem::path(preferenceRoot) / "preview.ini";
			SDL_free(preferenceRoot);

			ParticleDocumentTabs documents(false);
			std::string startupFailure;
			if (!startupPath.empty() && !documents.open(startupPath))
				startupFailure = "Could not open particle effect '" + startupPath.string() + "'.";
			if (documents.size() == 0u) documents.createNew();
			DiagnosticsView diagnostics;
			diagnostics.setOperationFailure(startupFailure);

			WindowSDL window("Particle Editor");
			window.create(1280, 800, false, true);
			mpp::Logger logger;
			if (!logger.initialise("ParticleEditor.log", mpp::Logger::Level::Debug))
				throw std::runtime_error("Could not create Particle Editor log.");
			std::unique_ptr<mpp::ResourceManager> resourceManager;
			mpp::RenderSystem renderSystem(window.getWidth(), window.getHeight(), &logger, options);
			resourceManager = std::make_unique<mpp::ResourceManager>(&renderSystem, &logger);
			resourceManager->setImageLoadFunction(mpp::app::loadImageFile);
			renderSystem.createCoreResources(resourceManager.get());
			ParticleResourceLibrary particleResources(resourceManager.get());
			if (editorConfig.resourceLibrary.empty())
				startupFailure = "No [Editor] resourceLibrary is configured; particle resource selectors are empty.";
			else if (!particleResources.reload(editorConfig.resourceRoot, editorConfig.resourceLibrary))
				startupFailure = "The configured particle resource library has errors; see Diagnostics.";
			diagnostics.setOperationFailure(startupFailure);

			ImGuiBackendData backend{};
			imGuiSetup(&renderSystem, resourceManager.get(), &backend, true);
			auto font = resourceManager->getResource("__ImGui_Font__", true);
			auto provider = std::make_shared<ImGuiDataProvider>(std::vector<mpp::ResourcePtr>{ font });
			mpp::BufferRenderer uiRenderer(provider);
			renderSystem.getOrCreateRenderPipeline("ParticleEditor.UI");

			ParticlePreview preview(&renderSystem, resourceManager.get());
			preview.setPreferencesPath(previewPreferencesPath);
			preview.initialise(resourcesPath, 900, 650);
			auto previewTexture = provider->registerTexture(preview.texture());
			ParticleDocument* installedDocument = nullptr;
			uint64_t installedRevision = 0;
			ParticleInspector inspector;
			mpp::app::AsyncParticleFileDialog fileDialog;
			enum class DialogPurpose { None, Open, Save };
			DialogPurpose dialogPurpose = DialogPurpose::None;
			size_t dialogDocument = 0;
			bool dialogClosesDocument = false;
			bool showDiagnostics = !startupFailure.empty();
			bool resetLayout = false;
			bool showAbout = false;
			bool running = true;
			bool exitSequence = false;
			std::optional<size_t> pendingClose;
			bool openUnsavedPrompt = false;
			std::optional<size_t> invalidSaveDocument;
			std::filesystem::path invalidSavePath;
			bool invalidSaveClosesDocument = false;
			bool openInvalidPrompt = false;
			std::optional<size_t> conflictSaveDocument;
			bool conflictSaveClosesDocument = false;
			std::optional<ParticleDocumentComparison> comparison;
			bool openComparison = false;
			bool lightGizmoDragging = false;
			InputManagerSDL input;
			TimerSDL timer;
			timer.reset();
			float fps = 0.0f;

			auto forgetInstalled = [&] { installedDocument = nullptr; installedRevision = 0; };
			auto eraseDocument = [&](size_t index)
			{
				if (index < documents.size())
				{
					if (&documents.at(index) == installedDocument) forgetInstalled();
					documents.discardAndClose(index);
				}
			};
			auto installActive = [&](bool force = false)
			{
				auto* document = documents.active();
				if (!document) return;
				if (force) document->publishPreviewNow();
				if (!force && installedDocument == document && installedRevision == document->previewRevision()) return;
				std::string failure;
				if (auto specification = document->previewSpecification())
				{
					bool const canUpdateLive = !force && installedDocument == document &&
						document->previewChange() == ParticlePreviewChange::Live;
					if (canUpdateLive && !preview.updateLive(*specification, &failure))
						preview.install(*specification, &failure);
					else if (!canUpdateLive) preview.install(*specification, &failure);
				}
				else failure = "This document has no valid preview state.";
				document->setPreviewFailure(failure);
				if (document->previewPaused()) preview.pauseSimulation(); else preview.resumeSimulation();
				preview.setSimulationTimeScale(document->previewTimeScale());
				installedDocument = document;
				installedRevision = document->previewRevision();
			};
			installActive(true);
			if (!preview.graphFailure().empty()) showDiagnostics = true;

			std::function<void(size_t, std::filesystem::path const&, bool, bool)> attemptSave;
			attemptSave = [&](size_t index, std::filesystem::path const& path, bool closesDocument, bool allowInvalid)
			{
				if (index >= documents.size()) return;
				try
				{
					auto result = documents.at(index).save(path, allowInvalid);
					if (result == ParticleSaveResult::Saved)
					{
						diagnostics.setOperationFailure({});
						if (closesDocument) eraseDocument(index);
					}
					else if (result == ParticleSaveResult::InvalidConfirmationRequired)
					{
						invalidSaveDocument = index;
						invalidSavePath = path;
						invalidSaveClosesDocument = closesDocument;
						openInvalidPrompt = true;
					}
					else
					{
						conflictSaveDocument = index;
						conflictSaveClosesDocument = closesDocument;
						documents.activate(index);
					}
				}
				catch (std::exception const& error)
				{
					diagnostics.setOperationFailure(error.what());
					showDiagnostics = true;
				}
			};

			auto beginSaveAs = [&](size_t index, bool closesDocument)
			{
				if (index >= documents.size() || fileDialog.busy()) return;
				dialogPurpose = DialogPurpose::Save;
				dialogDocument = index;
				dialogClosesDocument = closesDocument;
				auto& document = documents.at(index);
				auto suggested = document.hasPath() ? document.path() :
					(std::filesystem::current_path() / "Untitled Particle Effect.particle.yaml");
				fileDialog.save(window.getWindow(), suggested.string());
			};

			auto requestClose = [&](size_t index)
			{
				if (index >= documents.size()) return;
				if (documents.requestClose(index) == CloseRequestResult::UnsavedChanges)
				{
					documents.activate(index);
					pendingClose = index;
					openUnsavedPrompt = true;
				}
				else forgetInstalled();
			};

			while (running)
			{
				float delta = timer.getDeltaTime();
				if (delta > 0.0f) fps = fps == 0.0f ? 1.0f / delta : fps * 0.9f + (1.0f / delta) * 0.1f;
				bool closeRequested = !window.processEvents(&input);
				imGuiHandleInput(&input, &backend);
				input.update();
				if ((closeRequested || input.keyPressed(Key_Escape)) && !exitSequence)
					exitSequence = true;
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
							if (!documents.open(*result->path))
							{
								diagnostics.setOperationFailure("Could not open particle effect '" + *result->path + "'.");
								showDiagnostics = true;
							}
							else diagnostics.setOperationFailure({});
						}
						else if (dialogPurpose == DialogPurpose::Save)
							attemptSave(dialogDocument, *result->path, dialogClosesDocument, false);
					}
					dialogPurpose = DialogPurpose::None;
					dialogClosesDocument = false;
				}

				imGuiNewFrame(window.getWindow(), &backend);
				ImGui::NewFrame();
				bool requestNew = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N, ImGuiInputFlags_RouteGlobal);
				bool requestOpen = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal);
				bool requestSave = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal);
				bool requestSaveAs = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, ImGuiInputFlags_RouteGlobal);
				bool requestUndo = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal);
				bool requestRedo = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal);
				if (requestSaveAs) requestSave = false;

				auto* active = documents.active();
				if (ImGui::BeginMainMenuBar())
				{
					if (ImGui::BeginMenu("File"))
					{
						requestNew |= ImGui::MenuItem("New", "Ctrl+N");
						requestOpen |= ImGui::MenuItem("Open...", "Ctrl+O", false, !fileDialog.busy());
						requestSave |= ImGui::MenuItem("Save", "Ctrl+S", false, active != nullptr);
						requestSaveAs |= ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, active && !fileDialog.busy());
						if (ImGui::MenuItem("Close", "Ctrl+W", false, active != nullptr)) requestClose(documents.activeIndex());
						ImGui::Separator();
						if (ImGui::MenuItem("Exit")) exitSequence = true;
						ImGui::EndMenu();
					}
					active = documents.active();
					if (ImGui::BeginMenu("Edit"))
					{
						requestUndo |= ImGui::MenuItem("Undo", "Ctrl+Z", false, active && active->canUndo());
						requestRedo |= ImGui::MenuItem("Redo", "Ctrl+Y", false, active && active->canRedo());
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
						if (ImGui::MenuItem("Rebuild Preview", "F5", false, active != nullptr)) installActive(true);
						if (ImGui::BeginMenu("Preview Graph"))
						{
							for (auto const graph : { PreviewGraph::Pbr, PreviewGraph::Legacy })
							{
								bool const selected = preview.activeGraph() == graph;
								if (ImGui::MenuItem(graph == PreviewGraph::Pbr ? "PBR" : "Legacy", nullptr, selected) && !selected)
								{
									std::string failure;
									if (!preview.selectGraph(graph, &failure)) showDiagnostics = true;
								}
							}
							ImGui::EndMenu();
						}
						if (active)
						{
							if (active->previewPaused())
							{
								if (ImGui::MenuItem("Resume Simulation")) { active->setPreviewPaused(false); preview.resumeSimulation(); }
							}
							else if (ImGui::MenuItem("Pause Simulation")) { active->setPreviewPaused(true); preview.pauseSimulation(); }
							if (ImGui::MenuItem("Step Simulation", nullptr, false, preview.ready())) preview.stepSimulation();
							ImGui::Separator();
							if (ImGui::MenuItem("Focus Selection")) preview.focusSelection(active->specification(),
								active->hasSelectedEmitterTemplate() ? std::optional<size_t>(active->selectedEmitterTemplate()) : std::nullopt);
							if (ImGui::MenuItem("Frame Particle Effect Bounds")) preview.frameBounds();
							if (ImGui::MenuItem("Reset Camera")) preview.resetCamera();
						}
						ImGui::EndMenu();
					}
					if (ImGui::BeginMenu("Help"))
					{
						if (ImGui::MenuItem("About Particle Editor")) showAbout = true;
						ImGui::EndMenu();
					}
					ImGui::EndMainMenuBar();
				}
				if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_W, ImGuiInputFlags_RouteGlobal) && active)
					requestClose(documents.activeIndex());
				if (ImGui::Shortcut(ImGuiKey_F5, ImGuiInputFlags_RouteGlobal)) installActive(true);

				if (requestNew) { documents.createNew(); diagnostics.setOperationFailure({}); }
				if (requestOpen && !fileDialog.busy())
				{
					dialogPurpose = DialogPurpose::Open;
					fileDialog.open(window.getWindow());
				}
				active = documents.active();
				if (requestUndo && active) active->undo();
				if (requestRedo && active) active->redo();
				if (requestSave && active)
				{
					if (active->hasPath()) attemptSave(documents.activeIndex(), active->path(), false, false);
					else beginSaveAs(documents.activeIndex(), false);
				}
				if (requestSaveAs && active) beginSaveAs(documents.activeIndex(), false);

				if (ImGui::BeginViewportSideBar("##ParticleEditorToolbar", ImGui::GetMainViewport(), ImGuiDir_Up,
					ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y * 2.0f,
					ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
				{
					if (ImGui::Button("New")) documents.createNew();
					ImGui::SameLine();
					if (ImGui::Button("Open") && !fileDialog.busy()) { dialogPurpose = DialogPurpose::Open; fileDialog.open(window.getWindow()); }
					ImGui::SameLine();
					if (ImGui::Button("Save") && documents.active())
					{
						auto& document = *documents.active();
						if (document.hasPath()) attemptSave(documents.activeIndex(), document.path(), false, false);
						else beginSaveAs(documents.activeIndex(), false);
					}
					ImGui::SameLine();
					if (ImGui::Button("Undo") && documents.active()) documents.active()->undo();
					ImGui::SameLine();
					if (ImGui::Button("Redo") && documents.active()) documents.active()->redo();
					ImGui::SameLine();
					if (ImGui::Button("Rebuild Preview")) installActive(true);
					ImGui::SameLine();
					if (ImGui::Button(preview.activeGraph() == PreviewGraph::Pbr ? "Graph: PBR" : "Graph: Legacy"))
					{
						std::string failure;
						auto graph = preview.activeGraph() == PreviewGraph::Pbr ? PreviewGraph::Legacy : PreviewGraph::Pbr;
						if (!preview.selectGraph(graph, &failure)) showDiagnostics = true;
					}
					ImGui::SameLine();
					if (ImGui::Button("Frame Bounds")) preview.frameBounds();
					ImGui::SameLine();
					if (ImGui::Button("Reset Camera")) preview.resetCamera();
					active = documents.active();
					if (active)
					{
						ImGui::SameLine();
						if (ImGui::Button(active->previewPaused() ? "Resume" : "Pause"))
						{
							active->setPreviewPaused(!active->previewPaused());
							if (active->previewPaused()) preview.pauseSimulation(); else preview.resumeSimulation();
						}
						ImGui::SameLine();
						if (ImGui::Button("Step")) preview.stepSimulation();
						ImGui::SameLine();
						ImGui::SetNextItemWidth(110.0f);
						float timeScale = active->previewTimeScale();
						if (ImGui::SliderFloat("Time scale", &timeScale, 0.0f, 4.0f, "%.2fx"))
						{
							active->setPreviewTimeScale(timeScale);
							preview.setSimulationTimeScale(timeScale);
						}
					}
				}
				ImGui::End();

				if (ImGui::BeginViewportSideBar("##ParticleEditorStatus", ImGui::GetMainViewport(), ImGuiDir_Down,
					ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y * 2.0f,
					ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
				{
					active = documents.active();
					if (active)
						ImGui::Text("%s%s | %s | %s graph | %.1f FPS", active->path().empty() ? "Unsaved" : active->path().string().c_str(),
							active->dirty() ? " *" : "", diagnostics.statusText().c_str(),
							preview.activeGraph() == PreviewGraph::Pbr ? "PBR" : "legacy", fps);
					else ImGui::TextDisabled("No particle effect is open | %.1f FPS", fps);
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
					ImGui::DockBuilderDockWindow("Documents", left);
					ImGui::DockBuilderDockWindow("Particle Effect", left);
					ImGui::DockBuilderDockWindow("Diagnostics", left);
					ImGui::DockBuilderDockWindow("MPP Viewport", right);
					ImGui::DockBuilderFinish(dockspace);
				}

				ImGui::Begin("Documents");
				if (ImGui::BeginTabBar("ParticleEffectDocuments", ImGuiTabBarFlags_Reorderable))
				{
					std::optional<size_t> tabClose;
					for (size_t index = 0; index < documents.size(); ++index)
					{
						auto& document = documents.at(index);
						bool tabOpen = true;
						auto flags = document.dirty() ? ImGuiTabItemFlags_UnsavedDocument : ImGuiTabItemFlags_None;
						if (ImGui::BeginTabItem((document.displayName() + "##" + std::to_string(index)).c_str(), &tabOpen, flags))
						{
							documents.activate(index);
							ImGui::TextDisabled("%s", document.path().empty() ? "Not saved" : document.path().string().c_str());
							ImGui::EndTabItem();
						}
						if (!tabOpen) tabClose = index;
					}
					ImGui::EndTabBar();
					if (tabClose) requestClose(*tabClose);
				}
				if (documents.size() == 0u && ImGui::Button("New Particle Effect")) documents.createNew();
				ImGui::End();

				active = documents.active();
				if (active) inspector.draw(*active, particleResources, [&](std::filesystem::path const& path)
				{
					if (!documents.open(path))
					{
						diagnostics.setOperationFailure("Could not open child particle effect '" + path.string() + "'.");
						showDiagnostics = true;
					}
				});
				else { ImGui::Begin("Particle Effect"); ImGui::TextDisabled("No particle effect is open."); ImGui::End(); }

				active = documents.active();
				if (active)
				{
					auto combinedDiagnostics = active->diagnostics();
					combinedDiagnostics.append(particleResources.diagnostics());
					combinedDiagnostics.append(particleResources.referenceDiagnostics(
						active->specification(), active->path().string()));
					diagnostics.setDocumentDiagnostics(combinedDiagnostics);
					auto previewFailure = active->previewFailure();
					if (!preview.graphFailure().empty())
					{
						if (!previewFailure.empty()) previewFailure += " ";
						previewFailure += preview.graphFailure();
					}
					diagnostics.setPreviewFailure(previewFailure);
					diagnostics.setPreviewWarnings(preview.inputWarnings());
				}
				else
				{
					diagnostics.setDocumentDiagnostics(particleResources.diagnostics());
					diagnostics.setPreviewFailure(preview.graphFailure());
					diagnostics.setPreviewWarnings(preview.inputWarnings());
				}
				if (showDiagnostics) diagnostics.draw(&showDiagnostics);

				ImGui::Begin("MPP Viewport");
				int selectedGraph = preview.activeGraph() == PreviewGraph::Pbr ? 0 : 1;
				ImGui::SetNextItemWidth(110.0f);
				if (ImGui::Combo("Graph", &selectedGraph, "PBR\0Legacy\0"))
				{
					std::string failure;
					if (!preview.selectGraph(selectedGraph == 0 ? PreviewGraph::Pbr : PreviewGraph::Legacy, &failure))
						showDiagnostics = true;
				}
				ImGui::SameLine();
				auto& preferences = preview.preferences();
				int studioPreset = int(preferences.studioPreset);
				ImGui::SetNextItemWidth(120.0f);
				bool preferencesChanged = ImGui::Combo("Studio", &studioPreset, "Neutral\0Dark\0Warm\0");
				if (preferencesChanged) preferences.studioPreset = StudioPreset(studioPreset);
				ImGui::SameLine();
				preferencesChanged |= ImGui::Checkbox("Floor grid", &preferences.floorGrid);
				ImGui::SameLine();
				if (ImGui::Button("Focus Selection") && active)
					preview.focusSelection(active->specification(), active->hasSelectedEmitterTemplate() ?
						std::optional<size_t>(active->selectedEmitterTemplate()) : std::nullopt);
				ImGui::SameLine();
				if (ImGui::Button("Frame Bounds")) preview.frameBounds();
				ImGui::SameLine();
				if (ImGui::Button("Reset")) preview.resetCamera();

				if (ImGui::CollapsingHeader("Studio light"))
				{
					preferencesChanged |= ImGui::Checkbox("Enabled##StudioLight", &preferences.lightEnabled);
					ImGui::SameLine();
					preferencesChanged |= ImGui::Checkbox("Automatic orbit", &preferences.lightAutoOrbit);
					ImGui::SameLine();
					ImGui::SetNextItemWidth(130.0f);
					preferencesChanged |= ImGui::DragFloat("Orbit speed", &preferences.lightAutoOrbitSpeed, 0.01f, -5.0f, 5.0f, "%.2f rad/s");
					ImGui::BeginDisabled(!preferences.lightEnabled);
					float lightAzimuth = preferences.lightAzimuth * 57.2957795f;
					float lightElevation = preferences.lightElevation * 57.2957795f;
					ImGui::SetNextItemWidth(120.0f);
					if (ImGui::SliderFloat("Azimuth", &lightAzimuth, -180.0f, 180.0f, "%.1f deg"))
					{
						preferences.lightAzimuth = lightAzimuth * 0.0174532925f;
						preferencesChanged = true;
					}
					ImGui::SameLine();
					ImGui::SetNextItemWidth(120.0f);
					if (ImGui::SliderFloat("Elevation", &lightElevation, -85.0f, 85.0f, "%.1f deg"))
					{
						preferences.lightElevation = lightElevation * 0.0174532925f;
						preferencesChanged = true;
					}
					ImGui::SameLine();
					ImGui::SetNextItemWidth(115.0f);
					preferencesChanged |= ImGui::DragFloat("Distance", &preferences.lightDistance, 0.05f, 0.05f, 1000.0f, "%.2f");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(130.0f);
					preferencesChanged |= ImGui::ColorEdit3("Colour", &preferences.lightColour.x, ImGuiColorEditFlags_NoInputs);
					ImGui::SameLine();
					ImGui::SetNextItemWidth(115.0f);
					preferencesChanged |= ImGui::DragFloat("Intensity", &preferences.lightIntensity, 0.05f, 0.0f, 1000.0f, "%.2f");
					ImGui::EndDisabled();
				}
				if (ImGui::CollapsingHeader("Behaviour preview inputs", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto resourceInput = [&](char const* label, std::string& resource)
					{
						std::array<char, 512> buffer{};
						std::copy_n(resource.data(), std::min(resource.size(), buffer.size() - 1u), buffer.data());
						if (!ImGui::InputText(label, buffer.data(), buffer.size())) return false;
						resource = buffer.data();
						return true;
					};
					preferencesChanged |= resourceInput("Vector-field resource", preferences.vectorFieldResource);
					preferencesChanged |= resourceInput("Signed-distance-field resource", preferences.signedDistanceFieldResource);
					ImGui::TextDisabled("Use declared MPP logical names for 3D textures; these selectors are never serialized.");
					ImGui::SeparatorText("SDF world-to-texture transform (column-major)");
					for (size_t column = 0; column < 4u; ++column)
					{
						ImGui::PushID(int(column));
						preferencesChanged |= ImGui::InputFloat4("##SdfTransform", preferences.signedDistanceFieldTransform.data() + column * 4u, "%.5g");
						ImGui::PopID();
					}
					preferencesChanged |= ImGui::DragFloat("SDF distance scale", &preferences.signedDistanceFieldScale,
						0.01f, 0.0001f, 100000.0f, "%.5g", ImGuiSliderFlags_AlwaysClamp);
					preferencesChanged |= ImGui::DragFloat("SDF isovalue", &preferences.signedDistanceFieldIsoValue, 0.001f);
					preferencesChanged |= ImGui::Checkbox("Enable stable studio floor/wall collisions", &preferences.studioCollisions);
					if (!preferences.studioCollisions)
						ImGui::TextDisabled("Studio collisions are disabled by default and do not follow hidden visual faces.");
				}
				if (preferencesChanged) preview.applyPreferences();
				for (auto const& warning : preview.inputWarnings())
					ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.22f, 1.0f), "Warning: %s", warning.c_str());
				ImGui::TextDisabled("MMB / Alt+left: orbit | Shift: pan | Ctrl: zoom | Wheel: zoom | drag the light gizmo");
				auto available = ImGui::GetContentRegionAvail();
				uint32_t viewportWidth = std::max(64u, uint32_t(std::max(0.0f, available.x)));
				uint32_t viewportHeight = std::max(64u, uint32_t(std::max(0.0f, available.y)));
				try { preview.resize(viewportWidth, viewportHeight); }
				catch (std::exception const& error)
				{
					if (active) active->setPreviewFailure("Preview resize failed: " + std::string(error.what()));
				}
				auto viewportOrigin = ImGui::GetCursorScreenPos();
				ImGui::Image(previewTexture, ImVec2(float(viewportWidth), float(viewportHeight)), ImVec2(0, 1), ImVec2(1, 0));
				bool const viewportHovered = ImGui::IsItemHovered();
				auto& io = ImGui::GetIO();
				if (auto lightPosition = preview.lightViewportPosition())
				{
					ImVec2 marker(viewportOrigin.x + lightPosition->x, viewportOrigin.y + lightPosition->y);
					ImGui::GetWindowDrawList()->AddCircleFilled(marker, 7.0f, IM_COL32(255, 220, 120, 255));
					ImGui::GetWindowDrawList()->AddCircle(marker, 11.0f, IM_COL32(255, 255, 255, 220), 0, 2.0f);
					float const dx = io.MousePos.x - marker.x;
					float const dy = io.MousePos.y - marker.y;
					if (viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && dx * dx + dy * dy <= 196.0f)
						lightGizmoDragging = true;
				}
				if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) lightGizmoDragging = false;
				if (lightGizmoDragging)
					preview.manipulateLight(io.MouseDelta.x * 0.01f, -io.MouseDelta.y * 0.01f);
				else if (viewportHovered)
				{
					bool const middle = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
					bool const trackpad = io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left);
					if ((middle || trackpad) && io.KeyShift)
						preview.panCamera(-io.MouseDelta.x, io.MouseDelta.y);
					else if ((middle || trackpad) && io.KeyCtrl)
						preview.zoomCamera(io.MouseDelta.y * 0.01f);
					else if (middle || trackpad)
						preview.orbitCamera(-io.MouseDelta.x * 0.008f, -io.MouseDelta.y * 0.008f);
					if (io.MouseWheel != 0.0f) preview.zoomCamera(io.MouseWheel * std::log(0.85f));
				}
				ImGui::End();

				for (size_t index = 0; index < documents.size(); ++index)
				{
					auto state = documents.at(index).externalChangeState();
					if (state == ExternalChangeState::None) continue;
					ImGui::Begin(("External Change##" + std::to_string(index)).c_str());
					ImGui::TextWrapped("%s changed outside Particle Editor.", documents.at(index).path().string().c_str());
					if (state == ExternalChangeState::Conflict)
						ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "The editor version has unsaved changes. It will not be overwritten silently.");
					if (ImGui::Button(("Compare##" + std::to_string(index)).c_str()))
					{
						try { comparison = documents.at(index).compareWithDisk(); openComparison = true; }
						catch (std::exception const& error) { diagnostics.setOperationFailure(error.what()); showDiagnostics = true; }
					}
					ImGui::SameLine();
					if (ImGui::Button(("Reload from Disk##" + std::to_string(index)).c_str()))
					{
						forgetInstalled();
						if (!documents.at(index).reload()) { diagnostics.setOperationFailure("The changed particle effect could not be reloaded."); showDiagnostics = true; }
					}
					if (state == ExternalChangeState::Conflict)
					{
						ImGui::SameLine();
						if (ImGui::Button(("Keep Editor Version##" + std::to_string(index)).c_str()))
						{
							documents.at(index).keepEditorVersion();
							if (conflictSaveDocument && *conflictSaveDocument == index)
							{
								auto closes = conflictSaveClosesDocument;
								conflictSaveDocument.reset();
								attemptSave(index, documents.at(index).path(), closes, false);
							}
						}
					}
					ImGui::End();
				}

				if (openUnsavedPrompt) { ImGui::OpenPopup("Unsaved Particle Effect"); openUnsavedPrompt = false; }
				if (ImGui::BeginPopupModal("Unsaved Particle Effect", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
				{
					if (pendingClose && *pendingClose < documents.size())
					{
						auto index = *pendingClose;
						auto& document = documents.at(index);
						ImGui::TextWrapped("Save changes to %s before closing?", document.displayName().c_str());
						if (ImGui::Button("Save"))
						{
							pendingClose.reset();
							if (document.hasPath()) attemptSave(index, document.path(), true, false);
							else beginSaveAs(index, true);
							ImGui::CloseCurrentPopup();
						}
						ImGui::SameLine();
						if (ImGui::Button("Discard"))
						{
							pendingClose.reset();
							eraseDocument(index);
							ImGui::CloseCurrentPopup();
						}
						ImGui::SameLine();
						if (ImGui::Button("Cancel"))
						{
							pendingClose.reset();
							exitSequence = false;
							ImGui::CloseCurrentPopup();
						}
					}
					else ImGui::CloseCurrentPopup();
					ImGui::EndPopup();
				}

				if (openInvalidPrompt) { ImGui::OpenPopup("Invalid Particle Effect"); openInvalidPrompt = false; }
				if (ImGui::BeginPopupModal("Invalid Particle Effect", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::TextWrapped("This particle effect has production validation errors.\nIt can be saved, but the last valid live preview will remain active.");
					if (ImGui::Button("Save Invalid Document"))
					{
						if (invalidSaveDocument)
							attemptSave(*invalidSaveDocument, invalidSavePath, invalidSaveClosesDocument, true);
						invalidSaveDocument.reset();
						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();
					if (ImGui::Button("Cancel")) { invalidSaveDocument.reset(); exitSequence = false; ImGui::CloseCurrentPopup(); }
					ImGui::EndPopup();
				}

				if (openComparison) { ImGui::OpenPopup("Compare Particle Effect Versions"); openComparison = false; }
				if (ImGui::BeginPopupModal("Compare Particle Effect Versions", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
				{
					if (comparison)
					{
						ImGui::BeginChild("EditorVersion", ImVec2(420, 420), ImGuiChildFlags_Borders);
						ImGui::TextUnformatted("Editor version"); ImGui::Separator(); ImGui::TextUnformatted(comparison->editorYaml.c_str()); ImGui::EndChild();
						ImGui::SameLine();
						ImGui::BeginChild("DiskVersion", ImVec2(420, 420), ImGuiChildFlags_Borders);
						ImGui::TextUnformatted("Disk version"); ImGui::Separator(); ImGui::TextUnformatted(comparison->diskYaml.c_str()); ImGui::EndChild();
					}
					if (ImGui::Button("Close")) { comparison.reset(); ImGui::CloseCurrentPopup(); }
					ImGui::EndPopup();
				}

				if (showAbout) { ImGui::OpenPopup("About Particle Editor"); showAbout = false; }
				if (ImGui::BeginPopupModal("About Particle Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::TextUnformatted("Particle Editor\nCanonical version-2 particle effect authoring\nSafe tabbed workflows and live MPP preview");
					if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
					ImGui::EndPopup();
				}

				if (exitSequence && !pendingClose && !invalidSaveDocument && !fileDialog.busy())
				{
					if (documents.size() == 0u) running = false;
					else requestClose(0u);
				}

				active = documents.active();
				if (active) active->publishPreviewIfDue();
				installActive();
				active = documents.active();
				SDL_SetWindowTitle(window.getWindow(), active ?
					(active->displayName() + (active->dirty() ? " * - Particle Editor" : " - Particle Editor")).c_str() : "Particle Editor");
				ImGui::Render();
				provider->setDrawData(ImGui::GetDrawData());
				preview.update(delta);
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
