#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <Windows.h>
#include <sdl/SDL.h>
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "mpp/BufferRenderer.h"
#include "mpp/Camera.h"
#include "mpp/Colour.h"
#include "mpp/Logger.h"
#include "mpp/RenderSystem.h"
#include "mpp/RenderGraphStream.h"
#include "mpp/RenderPipeline.h"
#include "mpp/ResourceManager.h"
#include "mpp/Scene.h"
#include "mpp/app/FileDialog.h"
#include "mpp/app/ImGuiBackendData.h"
#include "mpp/app/ImGuiDataProvider.h"
#include "mpp/app/ImGuiPlatform.h"
#include "mpp/app/InputManagerSDL.h"
#include "mpp/app/TimerSDL.h"
#include "mpp/app/WindowSDL.h"
#include "mpp/resource-parsers/PbrPipelineParser.h"
#include "mpp/resource-parsers/PbrPipelineSerializer.h"
#include "mpp/resource-parsers/SceneParser.h"

using namespace mpp;

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	try
	{
		bool warningsAsErrors = false;
		if (__argc >= 3 && std::string(__argv[1]) == "--validate")
		{
			int pathIndex = 2;
			if (std::string(__argv[2]) == "--warnings-as-errors") { warningsAsErrors = true; pathIndex = 3; }
			if (__argc <= pathIndex) return 2;
			auto document = resource_parsers::PbrPipelineParser::fromFile(__argv[pathIndex]);
			auto diagnostics = document.validate();
			return diagnostics.hasErrors(warningsAsErrors) ? 1 : 0;
		}
		std::shared_ptr<PbrPipelineDocument> openDocument;
		std::shared_ptr<SceneDocument> openScene;
		std::string currentPath;
		bool recoveredDocument = false;
		if (__argc >= 2)
		{
			currentPath = __argv[1];
			std::string loadPath = currentPath;
			auto recoveryPath = std::filesystem::path(currentPath + ".recovery");
			if (std::filesystem::exists(recoveryPath) && std::filesystem::last_write_time(recoveryPath) > std::filesystem::last_write_time(currentPath) && MessageBoxA(nullptr,"A newer recovery document exists. Recover it?","PipelineEditor Recovery",MB_YESNO|MB_ICONQUESTION)==IDYES) { loadPath=recoveryPath.string(); recoveredDocument=true; }
			openDocument = std::make_shared<PbrPipelineDocument>(resource_parsers::PbrPipelineParser::fromFile(loadPath));
			if (!openDocument->previewScene.empty())
			{
				auto scenePath = std::filesystem::path(currentPath).parent_path() / openDocument->previewScene;
				openScene = std::make_shared<SceneDocument>(resource_parsers::SceneParser::fromFile(scenePath.string()));
			}
		}
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
		auto camera = std::make_shared<Camera>(glm::vec3(0, 3, 8), 0.0f, 0.0f, 0.0f, 60.0f, 1440.0f / 900.0f);
		renderSystem.getOrCreateRenderPipeline("EditorUI");
		std::string activePipeline = "EditorUI";
		if (openDocument && openDocument->graph)
		{
			auto graphStream = std::make_shared<RenderGraphStream>(&resources); graphStream->setGraph(openDocument->graph);
			auto graphResource = resources.declareResource("PipelineEditor.ActiveGraph", graphStream).first; graphResource->load(); graphResource->create();
			RenderPipelineOptions previewOptions; previewOptions.mode = RenderPipelineMode::XmlGraphPbrForward; previewOptions.graphTemplate = graphResource;
			renderSystem.getOrCreateRenderPipeline("EditorPreview", previewOptions); activePipeline = "EditorPreview";
		}
		int selectedPass = -1;
		auto loadWorkspace = [&](std::string const& path)
		{
			openDocument = std::make_shared<PbrPipelineDocument>(resource_parsers::PbrPipelineParser::fromFile(path));
			openScene.reset(); currentPath = path; selectedPass = -1;
			if (!openDocument->previewScene.empty()) openScene = std::make_shared<SceneDocument>(resource_parsers::SceneParser::fromFile((std::filesystem::path(path).parent_path() / openDocument->previewScene).string()));
		};
		bool running = true, pipelineDirty = recoveredDocument; float fps = 0, fpsTime = 0, recoveryTimer = 0; int frames = 0;
		while (running)
		{
			float dt = timer.getDeltaTime(); fpsTime += dt; recoveryTimer += dt; ++frames; if (fpsTime >= 0.5f) { fps = frames / fpsTime; frames = 0; fpsTime = 0; }
			if (pipelineDirty && openDocument && !currentPath.empty() && recoveryTimer >= 30.0f) { resource_parsers::PbrPipelineSerializer::toFile(*openDocument, currentPath + ".recovery"); recoveryTimer = 0; }
			window.processEvents(&input); imGuiHandleInput(&input, &backend); input.update(); if (input.keyPressed(Key_Escape)) running = false;
			imGuiNewFrame(window.getWindow(), &backend); ImGui::NewFrame();
			ImGuiID dockspace = ImGui::GetID("PipelineEditor.Dockspace");
			ImGui::DockSpaceOverViewport(dockspace, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
			static bool layoutBuilt = false;
			if (!layoutBuilt && (!ImGui::DockBuilderGetNode(dockspace) || !ImGui::DockBuilderGetNode(dockspace)->IsSplitNode()))
			{
				layoutBuilt = true; ImGui::DockBuilderRemoveNode(dockspace); ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace); ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->Size);
				ImGuiID left, right; ImGui::DockBuilderSplitNode(dockspace, ImGuiDir_Left, 0.28f, &left, &right); ImGuiID leftLower, leftUpper; ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.48f, &leftLower, &leftUpper);
				ImGui::DockBuilderDockWindow("Pipeline Hierarchy", leftUpper); ImGui::DockBuilderDockWindow("Inspector", leftLower); ImGui::DockBuilderDockWindow("Diagnostics", leftLower); ImGui::DockBuilderDockWindow("Allocations", leftLower); ImGui::DockBuilderDockWindow("Viewport", right); ImGui::DockBuilderDockWindow("Toolbar", right); ImGui::DockBuilderFinish(dockspace);
			}
			bool requestNew=false, requestOpen=false, requestSave=false;
			if (ImGui::BeginMainMenuBar()) { if (ImGui::BeginMenu("File")) { if(ImGui::MenuItem("New", "Ctrl+N"))requestNew=true; if(ImGui::MenuItem("Open...", "Ctrl+O"))requestOpen=true; if(ImGui::MenuItem("Save", "Ctrl+S",false,openDocument!=nullptr))requestSave=true; ImGui::Separator(); if (ImGui::MenuItem("Exit")) running=false; ImGui::EndMenu(); } if(ImGui::BeginMenu("Edit")){ImGui::MenuItem("Undo","Ctrl+Z");ImGui::MenuItem("Redo","Ctrl+Y");ImGui::EndMenu();} if(ImGui::BeginMenu("Pipeline")){ImGui::MenuItem("Validate");ImGui::MenuItem("Apply/Rebuild");ImGui::EndMenu();} ImGui::EndMainMenuBar(); }
			ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse); if(ImGui::Button("New"))requestNew=true; ImGui::SameLine(); if(ImGui::Button("Open"))requestOpen=true; ImGui::SameLine(); if(ImGui::Button("Save All")&&openDocument)requestSave=true; ImGui::SameLine(); ImGui::Button("Undo"); ImGui::SameLine(); ImGui::Button("Redo"); ImGui::SameLine(); ImGui::Button("Add Pass"); ImGui::SameLine(); ImGui::Button("Validate"); ImGui::SameLine(); ImGui::Button("Apply/Rebuild"); ImGui::End();
			if(requestNew){loadWorkspace("resources/FullPbrPipeline.xml");currentPath.clear();pipelineDirty=true;}
			if(requestOpen){if(auto path=mpp::app::openXmlFileDialog(window.getWindow(),"Open PBR Pipeline")){loadWorkspace(*path);pipelineDirty=false;}}
			if(requestSave&&openDocument){if(currentPath.empty()){auto path=mpp::app::saveXmlFileDialog(window.getWindow(),"Save PBR Pipeline","pipeline.xml");if(path)currentPath=*path;}if(!currentPath.empty()){resource_parsers::PbrPipelineSerializer::toFile(*openDocument,currentPath);pipelineDirty=false;std::filesystem::remove(currentPath+".recovery");}}
			ImGui::Begin("Pipeline Hierarchy"); ImGui::TextUnformatted(openDocument ? openDocument->name.c_str() : "Pipeline");
			if (openDocument && openDocument->graph && ImGui::TreeNodeEx("Passes", ImGuiTreeNodeFlags_DefaultOpen)) { for (uint32_t pass=0; pass<openDocument->graph->getPassCount(); ++pass) { auto info=openDocument->graph->getPassInfo({pass}); if(ImGui::Selectable(info.name.c_str(),selectedPass==(int)pass)) selectedPass=(int)pass; } ImGui::TreePop(); }
			if (openScene && ImGui::TreeNodeEx("Preview Scene", ImGuiTreeNodeFlags_DefaultOpen)) { for(auto const& model:openScene->models) ImGui::BulletText("%s",model.id.c_str()); ImGui::TreePop(); } ImGui::End();
			ImGui::Begin("Inspector");
			if(openDocument&&openDocument->graph&&selectedPass>=0)
			{
				auto info=openDocument->graph->getPassInfo({(uint32_t)selectedPass}); ImGui::Text("Pass: %s",info.name.c_str()); ImGui::Text("Factory: %s",info.callbackFactory.c_str());
				bool enabled=info.enabled; if(ImGui::Checkbox("Enabled",&enabled)){openDocument->graph->setPassEnabled({(uint32_t)selectedPass},enabled);pipelineDirty=true;}
				ImGui::Text("Inputs: %zu  Colour outputs: %zu  Depth outputs: %zu",info.sampledInputs.size(),info.colourOutputs.size(),info.depthOutputs.size());
				if(ImGui::CollapsingHeader("Uniform Parameters",ImGuiTreeNodeFlags_DefaultOpen))
				{
					bool changed=false;
					for(auto const& entry:info.parameters.getUniformData())
					{
						auto const& value=entry.second;
						if(value.count!=1) { ImGui::Text("%s [array %zu]",entry.first.c_str(),value.count); continue; }
						if(value.type==program::GLSLType::Float && value.numElements>=1 && value.numElements<=4) { float values[4]{}; memcpy(values,value.data,value.numElements*sizeof(float)); bool edited=value.numElements==1?ImGui::InputFloat(entry.first.c_str(),values):value.numElements==2?ImGui::InputFloat2(entry.first.c_str(),values):value.numElements==3?ImGui::InputFloat3(entry.first.c_str(),values):ImGui::InputFloat4(entry.first.c_str(),values); if(edited){info.parameters.setUniform(entry.first,value.type,1,value.numElements,reinterpret_cast<char const*>(values));changed=true;} }
						else if(value.type==program::GLSLType::Int || value.type==program::GLSLType::Bool) { int current=*reinterpret_cast<int const*>(value.data); if(ImGui::InputInt(entry.first.c_str(),&current)){info.parameters.setUniform(entry.first,value.type,1,1,reinterpret_cast<char const*>(&current));changed=true;} }
						else ImGui::Text("%s (reflected type %d)",entry.first.c_str(),(int)value.type);
					}
					if(changed){openDocument->graph->setPassParameters({(uint32_t)selectedPass},info.parameters);pipelineDirty=true;}
				}
			}
			else ImGui::TextUnformatted("Select a pipeline pass."); ImGui::End();
			ImGui::Begin("Diagnostics"); if (openDocument) { auto diagnostics=openDocument->validate(); ImGui::Text("%zu error(s), %zu warning(s)",diagnostics.count(DiagnosticSeverity::Error),diagnostics.count(DiagnosticSeverity::Warning)); for(auto const&d:diagnostics.getDiagnostics()) ImGui::BulletText("[%s] %s",d.code.c_str(),d.message.c_str()); } else ImGui::TextColored(ImVec4(0.3f,1,0.4f,1),"No document loaded"); ImGui::End();
			ImGui::Begin("Allocations"); if(openDocument&&openDocument->graph){auto plan=openDocument->graph->buildAllocationPlan({1280,720});if(plan.valid){ImGui::Text("Physical estimate: %.2f MiB",plan.estimatedPhysicalBytes/1048576.0);for(auto const& image:plan.allocatedImages)ImGui::BulletText("%s.v%u -> allocation %u, %.1f KiB, passes %u-%u",image.debugName.c_str(),image.image.version,image.physicalAllocation,image.estimatedBytes/1024.0,image.firstPass,image.lastPass);}else ImGui::TextUnformatted("Allocation unavailable while graph is invalid.");} ImGui::End();
			ImGui::Begin("Viewport"); ImGui::TextUnformatted("PBR preview target will be presented here."); ImGui::End();
			ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 24)); ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x,24)); ImGui::Begin("##Status",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings); ImGui::Text("%s | %.1f FPS | %d triangles | Preview: %s",openDocument?"Loaded":"Ready",fps,renderSystem.getCurrentRenderInfo().trianglesRendered,openDocument?"document loaded":"no document"); ImGui::End();
			ImGui::Render(); provider->setDrawData(ImGui::GetDrawData());
			renderSystem.startStatsCollection(); renderSystem.renderScene(scene, camera, glm::vec2(0), activePipeline); renderer.render(&renderSystem); renderSystem.finishStatsCollection(); window.show();
		}
		scene->unload(); imGuiShutdown(&backend); renderSystem.destroyCoreResources(); window.destroy(); SDL_Quit();
		return 0;
	}
	catch (std::exception const& error) { MessageBoxA(nullptr, error.what(), "PipelineEditor Error", MB_ICONERROR); return 1; }
}
