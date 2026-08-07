#include <memory>
#include <stdexcept>
#include <vector>
#include <Windows.h>
#include <sdl/SDL.h>
#include "imgui/imgui.h"
#include "mpp/BufferRenderer.h"
#include "mpp/Camera.h"
#include "mpp/Colour.h"
#include "mpp/Logger.h"
#include "mpp/RenderSystem.h"
#include "mpp/ResourceManager.h"
#include "mpp/Scene.h"
#include "mpp/app/ImGuiBackendData.h"
#include "mpp/app/ImGuiDataProvider.h"
#include "mpp/app/ImGuiPlatform.h"
#include "mpp/app/InputManagerSDL.h"
#include "mpp/app/TimerSDL.h"
#include "mpp/app/WindowSDL.h"

using namespace mpp;

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	try
	{
		if (SDL_Init(SDL_INIT_VIDEO) < 0) throw std::runtime_error(SDL_GetError());
		WindowSDL window("PBR Pipeline Editor"); window.create(1440, 900, false, true);
		Logger logger; if (!logger.initialise("PipelineEditor.log", Logger::Level::Debug)) throw std::runtime_error("Could not create editor log.");
		RenderSystem renderSystem(window.getWidth(), window.getHeight(), &logger);
		ResourceManager resources(&renderSystem, &logger); renderSystem.createCoreResources(&resources);
		ImGuiBackendData backend{}; imGuiSetup(&renderSystem, &resources, &backend, true);
		InputManagerSDL input; TimerSDL timer; timer.reset();
		auto font = resources.getResource("__ImGui_Font__", true);
		auto provider = std::make_shared<ImGuiDataProvider>(std::vector<ResourcePtr>{ font });
		BufferRenderer renderer(provider);
		auto scene = std::make_shared<Scene>(&renderSystem); scene->load(); scene->setClearColour(Colour(0.094f, 0.106f, 0.125f));
		auto camera = std::make_shared<Camera>(glm::vec3(0, 3, 8), 0, 0, 0, 60, 1440.0f / 900.0f);
		renderSystem.getOrCreateRenderPipeline("EditorUI");
		bool running = true; float fps = 0, fpsTime = 0; int frames = 0;
		while (running)
		{
			float dt = timer.getDeltaTime(); fpsTime += dt; ++frames; if (fpsTime >= 0.5f) { fps = frames / fpsTime; frames = 0; fpsTime = 0; }
			window.processEvents(&input); imGuiHandleInput(&input, &backend); input.update(); if (input.keyPressed(Key_Escape)) running = false;
			imGuiNewFrame(window.getWindow(), &backend); ImGui::NewFrame();
			ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
			if (ImGui::BeginMainMenuBar()) { if (ImGui::BeginMenu("File")) { ImGui::MenuItem("New", "Ctrl+N"); ImGui::MenuItem("Open...", "Ctrl+O"); ImGui::MenuItem("Save", "Ctrl+S"); ImGui::Separator(); if (ImGui::MenuItem("Exit")) running=false; ImGui::EndMenu(); } if(ImGui::BeginMenu("Edit")){ImGui::MenuItem("Undo","Ctrl+Z");ImGui::MenuItem("Redo","Ctrl+Y");ImGui::EndMenu();} if(ImGui::BeginMenu("Pipeline")){ImGui::MenuItem("Validate");ImGui::MenuItem("Apply/Rebuild");ImGui::EndMenu();} ImGui::EndMainMenuBar(); }
			ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse); ImGui::Button("New"); ImGui::SameLine(); ImGui::Button("Open"); ImGui::SameLine(); ImGui::Button("Save All"); ImGui::SameLine(); ImGui::Button("Undo"); ImGui::SameLine(); ImGui::Button("Redo"); ImGui::SameLine(); ImGui::Button("Add Pass"); ImGui::SameLine(); ImGui::Button("Validate"); ImGui::SameLine(); ImGui::Button("Apply/Rebuild"); ImGui::End();
			ImGui::Begin("Pipeline Hierarchy"); ImGui::TextUnformatted("Pipeline"); ImGui::BulletText("Settings"); ImGui::BulletText("Imports"); ImGui::BulletText("Images"); ImGui::BulletText("Materials"); ImGui::BulletText("Passes"); ImGui::End();
			ImGui::Begin("Inspector"); ImGui::TextUnformatted("Select a pipeline or scene item to edit its properties."); ImGui::End();
			ImGui::Begin("Diagnostics"); ImGui::TextColored(ImVec4(0.3f,1,0.4f,1),"No errors"); ImGui::End();
			ImGui::Begin("Viewport"); ImGui::TextUnformatted("PBR preview target will be presented here."); ImGui::End();
			ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 24)); ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x,24)); ImGui::Begin("##Status",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings); ImGui::Text("Ready | %.1f FPS | 0 triangles | Preview: no document",fps); ImGui::End();
			ImGui::Render(); provider->setDrawData(ImGui::GetDrawData());
			renderSystem.startStatsCollection(); renderSystem.renderScene(scene, camera, glm::vec2(0), "EditorUI"); renderer.render(&renderSystem); renderSystem.finishStatsCollection(); window.show();
		}
		scene->unload(); imGuiShutdown(&backend); renderSystem.destroyCoreResources(); window.destroy(); SDL_Quit();
		return 0;
	}
	catch (std::exception const& error) { MessageBoxA(nullptr, error.what(), "PipelineEditor Error", MB_ICONERROR); return 1; }
}
