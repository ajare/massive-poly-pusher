#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
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
#include "mpp/resource-parsers/PbrPipelineDocumentLoader.h"
#include "mpp/resource-parsers/PbrPipelineSerializer.h"
#include "mpp/resource-parsers/SceneParser.h"
#include "mpp/resource-parsers/SceneSerializer.h"

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
			auto document = resource_parsers::PbrPipelineDocumentLoader::fromFile(__argv[pathIndex]);
			auto diagnostics = document.validate();
			if(!document.previewScene.empty())
			{
				auto sceneFile=std::filesystem::path(__argv[pathIndex]).parent_path()/document.previewScene;
				if(std::filesystem::exists(sceneFile))diagnostics.append(resource_parsers::SceneParser::fromFile(sceneFile.string()).validate());
				else diagnostics.error("MPP-PIPELINE-CLI-001","Preview scene does not exist: "+sceneFile.string(),{__argv[pathIndex]},"previewScene");
			}
			for(auto const& value:diagnostics.getDiagnostics())fprintf(stderr,"%s: %s\n",value.code.c_str(),value.message.c_str());
			return diagnostics.hasErrors(warningsAsErrors) ? 1 : 0;
		}
		std::shared_ptr<PbrPipelineDocument> openDocument;
		std::shared_ptr<SceneDocument> openScene;
		std::string currentPath, scenePath;
		std::vector<std::string> recentPaths;{std::ifstream recent("PipelineEditor.recent.txt");std::string path;while(std::getline(recent,path))if(!path.empty())recentPaths.push_back(path);}
		auto rememberRecent=[&](std::string const& path){recentPaths.erase(std::remove(recentPaths.begin(),recentPaths.end(),path),recentPaths.end());recentPaths.insert(recentPaths.begin(),path);if(recentPaths.size()>8)recentPaths.resize(8);std::ofstream recent("PipelineEditor.recent.txt",std::ios::trunc);for(auto const& value:recentPaths)recent<<value<<'\n';};
		bool recoveredDocument = false;
		if (__argc >= 2)
		{
			currentPath = __argv[1]; rememberRecent(currentPath);
			std::string loadPath = currentPath;
			auto recoveryPath = std::filesystem::path(currentPath + ".recovery");
			if (std::filesystem::exists(recoveryPath) && std::filesystem::last_write_time(recoveryPath) > std::filesystem::last_write_time(currentPath) && MessageBoxA(nullptr,"A newer recovery document exists. Recover it?","PipelineEditor Recovery",MB_YESNO|MB_ICONQUESTION)==IDYES) { loadPath=recoveryPath.string(); recoveredDocument=true; }
			openDocument = std::make_shared<PbrPipelineDocument>(resource_parsers::PbrPipelineDocumentLoader::fromFile(loadPath));
			if (!openDocument->previewScene.empty())
			{
				scenePath = (std::filesystem::path(currentPath).parent_path() / openDocument->previewScene).string();
				openScene = std::make_shared<SceneDocument>(resource_parsers::SceneParser::fromFile(scenePath));
			}
			if (openDocument->importedFromRenderGraph) currentPath.clear();
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
		std::string activePipeline = "EditorUI", activeGraphResource, previewFailure; bool previewStale=false; uint32_t runtimeGeneration = 0;
		auto rebuildPreview = [&]()
		{
			if (!openDocument || !openDocument->graph || openDocument->validate().hasErrors()){previewStale=!activeGraphResource.empty();previewFailure="Working document is invalid; retaining the last valid preview.";return false;}
			auto suffix = std::to_string(++runtimeGeneration), candidatePipeline="EditorPreview."+suffix, candidateGraph="PipelineEditor.Graph."+suffix;bool graphDeclared=false,pipelineDeclared=false;
			try
			{
				auto graphStream=std::make_shared<RenderGraphStream>(&resources);graphStream->setGraph(openDocument->graph);auto graphResource=resources.declareResource(candidateGraph,graphStream).first;graphDeclared=true;graphResource->load();graphResource->create();RenderPipelineOptions previewOptions;previewOptions.mode=RenderPipelineMode::XmlGraphPbrForward;previewOptions.graphTemplate=graphResource;renderSystem.getOrCreateRenderPipeline(candidatePipeline,previewOptions);pipelineDeclared=true;
				auto obsoletePipeline=activePipeline,obsoleteGraph=activeGraphResource;activePipeline=candidatePipeline;activeGraphResource=candidateGraph;previewStale=false;previewFailure.clear();if(!obsoleteGraph.empty()){renderSystem.removeRenderPipeline(obsoletePipeline);resources.deleteResource(obsoleteGraph);}return true;
			}
			catch(std::exception const& error)
			{
				if(pipelineDeclared)renderSystem.removeRenderPipeline(candidatePipeline);if(graphDeclared){try{resources.deleteResource(candidateGraph);}catch(...){}}previewStale=!activeGraphResource.empty();previewFailure=error.what();return false;
			}
		};
		rebuildPreview();
		int selectedPass = -1, selectedModel = -1, selectedLocalResource = -1, selectedExternalResource = -1;
		auto loadWorkspace = [&](std::string const& path)
		{
			openDocument = std::make_shared<PbrPipelineDocument>(resource_parsers::PbrPipelineDocumentLoader::fromFile(path));
			openScene.reset(); scenePath.clear(); currentPath = openDocument->importedFromRenderGraph ? std::string() : path; rememberRecent(path); selectedPass = -1; selectedModel = -1; selectedLocalResource = -1; selectedExternalResource = -1;
			if (!openDocument->previewScene.empty()) { scenePath=(std::filesystem::path(path).parent_path()/openDocument->previewScene).string(); openScene = std::make_shared<SceneDocument>(resource_parsers::SceneParser::fromFile(scenePath)); }
		};
		bool running = true, pipelineDirty = recoveredDocument, sceneDirty = false, resetLayout = true; float fps = 0, fpsTime = 0, recoveryTimer = 0; int frames = 0;
		while (running)
		{
			float dt = timer.getDeltaTime(); fpsTime += dt; recoveryTimer += dt; ++frames; if (fpsTime >= 0.5f) { fps = frames / fpsTime; frames = 0; fpsTime = 0; }
			if (pipelineDirty && openDocument && !currentPath.empty() && recoveryTimer >= 30.0f) { resource_parsers::PbrPipelineSerializer::toFile(*openDocument, currentPath + ".recovery"); recoveryTimer = 0; }
			bool closeRequested = !window.processEvents(&input); imGuiHandleInput(&input, &backend); input.update(); if (input.keyPressed(Key_Escape)) closeRequested = true;
			if(closeRequested){if(!(pipelineDirty||sceneDirty)||MessageBoxA(nullptr,"Discard unsaved document changes?","PipelineEditor",MB_YESNO|MB_ICONWARNING)==IDYES)running=false;}
			imGuiNewFrame(window.getWindow(), &backend); ImGui::NewFrame();
			ImGuiID dockspace = ImGui::GetID("PipelineEditor.Dockspace");
			ImGui::DockSpaceOverViewport(dockspace, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
			if (resetLayout || !ImGui::DockBuilderGetNode(dockspace) || !ImGui::DockBuilderGetNode(dockspace)->IsSplitNode())
			{
				resetLayout = false; ImGui::DockBuilderRemoveNode(dockspace); ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace); ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->Size);
				ImGuiID left, right; ImGui::DockBuilderSplitNode(dockspace, ImGuiDir_Left, 0.28f, &left, &right); ImGuiID leftLower, leftUpper; ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.48f, &leftLower, &leftUpper);
				ImGui::DockBuilderDockWindow("Pipeline Hierarchy", leftUpper); ImGui::DockBuilderDockWindow("Inspector", leftLower); ImGui::DockBuilderDockWindow("Diagnostics", leftLower); ImGui::DockBuilderDockWindow("Allocations", leftLower); ImGui::DockBuilderDockWindow("Viewport", right); ImGui::DockBuilderDockWindow("Toolbar", right); ImGui::DockBuilderFinish(dockspace);
			}
			bool requestNew=false, requestOpen=false, requestSave=false, requestSaveScene=false;std::string requestedRecent;
			if (ImGui::BeginMainMenuBar()) { if (ImGui::BeginMenu("File")) { if(ImGui::MenuItem("New", "Ctrl+N"))requestNew=true; if(ImGui::MenuItem("Open...", "Ctrl+O"))requestOpen=true; if(!recentPaths.empty()&&ImGui::BeginMenu("Open Recent")){for(auto const& path:recentPaths)if(ImGui::MenuItem(path.c_str()))requestedRecent=path;ImGui::EndMenu();} if(ImGui::MenuItem("Save", "Ctrl+S",false,openDocument!=nullptr))requestSave=true; if(ImGui::MenuItem("Save Scene",nullptr,false,openScene!=nullptr))requestSaveScene=true; ImGui::Separator(); if (ImGui::MenuItem("Exit")) { if(!(pipelineDirty||sceneDirty)||MessageBoxA(nullptr,"Discard unsaved document changes?","PipelineEditor",MB_YESNO|MB_ICONWARNING)==IDYES)running=false; } ImGui::EndMenu(); } if(ImGui::BeginMenu("Edit")){ImGui::MenuItem("Undo","Ctrl+Z");ImGui::MenuItem("Redo","Ctrl+Y");ImGui::EndMenu();} if(ImGui::BeginMenu("Pipeline")){ImGui::MenuItem("Validate");ImGui::MenuItem("Apply/Rebuild");ImGui::EndMenu();} if(ImGui::BeginMenu("Window")){if(ImGui::MenuItem("Reset Layout"))resetLayout=true;ImGui::EndMenu();} ImGui::EndMainMenuBar(); }
			ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse); if(ImGui::Button("New"))requestNew=true; ImGui::SameLine(); if(ImGui::Button("Open"))requestOpen=true; ImGui::SameLine(); if(ImGui::Button("Save All")&&openDocument){requestSave=true;requestSaveScene=openScene!=nullptr;} ImGui::SameLine(); ImGui::Button("Undo"); ImGui::SameLine(); ImGui::Button("Redo"); ImGui::SameLine(); ImGui::Button("Add Pass"); ImGui::SameLine(); ImGui::Button("Validate"); ImGui::SameLine(); if(ImGui::Button("Apply/Rebuild"))rebuildPreview(); ImGui::End();
			if(requestNew&&(!(pipelineDirty||sceneDirty)||MessageBoxA(nullptr,"Discard unsaved document changes?","PipelineEditor",MB_YESNO|MB_ICONWARNING)==IDYES)){loadWorkspace("resources/FullPbrPipeline.xml");currentPath.clear();pipelineDirty=true;rebuildPreview();}
			if(requestOpen&&(!(pipelineDirty||sceneDirty)||MessageBoxA(nullptr,"Discard unsaved document changes?","PipelineEditor",MB_YESNO|MB_ICONWARNING)==IDYES)){if(auto path=mpp::app::openXmlFileDialog(window.getWindow(),"Open PBR Pipeline")){loadWorkspace(*path);pipelineDirty=false;sceneDirty=false;rebuildPreview();}}
			if(!requestedRecent.empty()&&(!(pipelineDirty||sceneDirty)||MessageBoxA(nullptr,"Discard unsaved document changes?","PipelineEditor",MB_YESNO|MB_ICONWARNING)==IDYES)){if(std::filesystem::exists(requestedRecent)){loadWorkspace(requestedRecent);pipelineDirty=false;sceneDirty=false;rebuildPreview();}else{recentPaths.erase(std::remove(recentPaths.begin(),recentPaths.end(),requestedRecent),recentPaths.end());std::ofstream recent("PipelineEditor.recent.txt",std::ios::trunc);for(auto const& value:recentPaths)recent<<value<<'\n';MessageBoxA(nullptr,"The recent pipeline no longer exists and was removed from the list.","PipelineEditor",MB_OK|MB_ICONWARNING);}}
			if(requestSaveScene&&openScene){if(scenePath.empty()){auto path=mpp::app::saveXmlFileDialog(window.getWindow(),"Save Preview Scene","preview.scene.xml");if(path)scenePath=*path;}if(!scenePath.empty()){resource_parsers::SceneSerializer::toFile(*openScene,scenePath);sceneDirty=false;}}
			if(requestSave&&openDocument){if(currentPath.empty()){auto path=mpp::app::saveXmlFileDialog(window.getWindow(),"Save PBR Pipeline","pipeline.xml");if(path)currentPath=*path;}if(!currentPath.empty()){resource_parsers::PbrPipelineSerializer::toFile(*openDocument,currentPath);rememberRecent(currentPath);pipelineDirty=false;std::filesystem::remove(currentPath+".recovery");}}
			ImGui::Begin("Pipeline Hierarchy"); ImGui::TextUnformatted(openDocument ? openDocument->name.c_str() : "Pipeline");
			if (openDocument && openDocument->graph && ImGui::TreeNodeEx("Passes", ImGuiTreeNodeFlags_DefaultOpen)) { for (uint32_t pass=0; pass<openDocument->graph->getPassCount(); ++pass) { auto info=openDocument->graph->getPassInfo({pass}); if(ImGui::Selectable(info.name.c_str(),selectedPass==(int)pass)){selectedPass=(int)pass;selectedModel=-1;selectedLocalResource=-1;selectedExternalResource=-1;} } ImGui::TreePop(); }
			if(openDocument&&!openDocument->localResources.empty()&&ImGui::TreeNodeEx("Local Resources",ImGuiTreeNodeFlags_DefaultOpen)){for(size_t index=0;index<openDocument->localResources.size();++index)if(ImGui::Selectable(openDocument->localResources[index].name.c_str(),selectedLocalResource==(int)index)){selectedLocalResource=(int)index;selectedExternalResource=-1;selectedPass=-1;selectedModel=-1;}ImGui::TreePop();}
			if(openDocument&&!openDocument->externalResources.empty()&&ImGui::TreeNodeEx("External Libraries",ImGuiTreeNodeFlags_DefaultOpen)){for(size_t index=0;index<openDocument->externalResources.size();++index){auto const& value=openDocument->externalResources[index];auto label=value.libraryName+"::"+value.resource.name;if(ImGui::Selectable(label.c_str(),selectedExternalResource==(int)index)){selectedExternalResource=(int)index;selectedLocalResource=-1;selectedPass=-1;selectedModel=-1;}}ImGui::TreePop();}
			if (openScene && ImGui::TreeNodeEx("Preview Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
				if(ImGui::Selectable("Camera",selectedModel==-2)){selectedModel=-2;selectedPass=-1;selectedLocalResource=-1;selectedExternalResource=-1;}
				if(ImGui::Selectable("Environment",selectedModel==-3)){selectedModel=-3;selectedPass=-1;selectedLocalResource=-1;selectedExternalResource=-1;}
				if(ImGui::TreeNodeEx("Models",ImGuiTreeNodeFlags_DefaultOpen)){for(size_t model=0;model<openScene->models.size();++model)if(ImGui::Selectable(openScene->models[model].id.c_str(),selectedModel==(int)model)){selectedModel=(int)model;selectedPass=-1;selectedLocalResource=-1;selectedExternalResource=-1;}ImGui::TreePop();}
				if(ImGui::TreeNodeEx("Lights",ImGuiTreeNodeFlags_DefaultOpen)){for(size_t light=0;light<openScene->lights.size();++light)if(ImGui::Selectable(openScene->lights[light].id.c_str(),selectedModel==-100-(int)light)){selectedModel=-100-(int)light;selectedPass=-1;selectedLocalResource=-1;selectedExternalResource=-1;}ImGui::TreePop();}
				ImGui::TreePop(); } ImGui::End();
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
			else if(openDocument&&selectedLocalResource>=0&&(size_t)selectedLocalResource<openDocument->localResources.size())
			{
				auto const& value=openDocument->localResources[(size_t)selectedLocalResource];ImGui::Text("Local resource: %s",value.name.c_str());ImGui::Text("Type: %s",value.definition.getName().c_str());ImGui::TextUnformatted("Editable pipeline-owned resource");
			}
			else if(openDocument&&selectedExternalResource>=0&&(size_t)selectedExternalResource<openDocument->externalResources.size())
			{
				auto const& value=openDocument->externalResources[(size_t)selectedExternalResource];auto qualified=value.libraryName+"::"+value.resource.name;ImGui::Text("External resource: %s",qualified.c_str());ImGui::Text("Library: %s",value.libraryPath.c_str());ImGui::TextDisabled("Read-only");if(ImGui::Button("Make Local Copy")){auto localName=value.resource.name+".Local";unsigned suffix=2;while(std::any_of(openDocument->localResources.begin(),openDocument->localResources.end(),[&](auto const& current){return current.name==localName;}))localName=value.resource.name+".Local"+std::to_string(suffix++);if(openDocument->makeLocalCopy(qualified,localName)){selectedLocalResource=(int)openDocument->localResources.size()-1;selectedExternalResource=-1;pipelineDirty=true;}}
			}
			else if(openScene&&selectedModel>=0)
			{
				auto& model=openScene->models[(size_t)selectedModel]; ImGui::Text("Model: %s",model.id.c_str()); bool changed=false;
				changed|=ImGui::InputFloat3("Translation",&model.translation.x); changed|=ImGui::InputFloat3("Rotation (degrees)",&model.rotationDegrees.x); changed|=ImGui::InputFloat3("Scale",&model.scale.x);
				if(model.source==SceneModelSource::Box){changed|=ImGui::InputFloat("Width",&model.primitive.width);changed|=ImGui::InputFloat("Height",&model.primitive.height);changed|=ImGui::InputFloat("Depth",&model.primitive.depth);}else if(model.source==SceneModelSource::Sphere){changed|=ImGui::InputFloat("Radius",&model.primitive.radius);int resolution=(int)model.primitive.resolution;if(ImGui::InputInt("Resolution",&resolution)){model.primitive.resolution=(uint32_t)std::max(0,resolution);changed=true;}}else if(model.source==SceneModelSource::Cylinder){changed|=ImGui::InputFloat("Length",&model.primitive.height);changed|=ImGui::InputFloat("Bottom radius",&model.primitive.radius);changed|=ImGui::InputFloat("Top radius",&model.primitive.topRadius);int resolution=(int)model.primitive.resolution;if(ImGui::InputInt("Resolution",&resolution)){model.primitive.resolution=(uint32_t)std::max(0,resolution);changed=true;}}else if(model.source==SceneModelSource::Grid){changed|=ImGui::InputFloat("Width",&model.primitive.width);changed|=ImGui::InputFloat("Depth",&model.primitive.depth);int x=(int)model.primitive.segmentsX,z=(int)model.primitive.segmentsZ;if(ImGui::InputInt("X segments",&x)){model.primitive.segmentsX=(uint32_t)std::max(0,x);changed=true;}if(ImGui::InputInt("Z segments",&z)){model.primitive.segmentsZ=(uint32_t)std::max(0,z);changed=true;}changed|=ImGui::InputFloat("Texture repeat U",&model.primitive.textureRepeatU);changed|=ImGui::InputFloat("Texture repeat V",&model.primitive.textureRepeatV);}
				changed|=ImGui::Checkbox("Visible",&model.visible); changed|=ImGui::Checkbox("Shadow caster",&model.shadowCaster);
				char binding[256]{}; strncpy_s(binding,model.materialBinding.c_str(),255); if(ImGui::InputText("Material binding",binding,sizeof(binding))){model.materialBinding=binding;changed=true;}
				char layers[256]{}; std::string layerText; for(auto const& layer:model.layers){if(!layerText.empty())layerText+=",";layerText+=layer;} strncpy_s(layers,layerText.c_str(),255); if(ImGui::InputText("Layers (comma separated)",layers,sizeof(layers))){model.layers.clear();std::stringstream stream(layers);std::string layer;while(std::getline(stream,layer,','))if(!layer.empty())model.layers.push_back(layer);changed=true;}
				if(changed)sceneDirty=true;
			}
			else if(openScene&&selectedModel==-2)
			{
				auto& value=openScene->camera; ImGui::TextUnformatted("Camera"); bool changed=false; changed|=ImGui::InputFloat3("Position",&value.position.x);changed|=ImGui::InputFloat3("Target",&value.target.x);changed|=ImGui::InputFloat("Vertical FOV",&value.fov);changed|=ImGui::InputFloat("Near plane",&value.nearPlane);changed|=ImGui::InputFloat("Far plane",&value.farPlane);if(changed)sceneDirty=true;
			}
			else if(openScene&&selectedModel==-3)
			{
				char value[256]{};strncpy_s(value,openScene->environmentBinding.c_str(),255);if(ImGui::InputText("Environment binding",value,sizeof(value))){openScene->environmentBinding=value;sceneDirty=true;}
			}
			else if(openScene&&selectedModel<=-100)
			{
				auto index=(size_t)(-100-selectedModel);if(index<openScene->lights.size()){auto& value=openScene->lights[index];ImGui::Text("Light: %s",value.id.c_str());bool changed=false;int type=value.type==SceneLightType::Point?1:0;if(ImGui::Combo("Type",&type,"Directional\0Point\0")){value.type=type?SceneLightType::Point:SceneLightType::Directional;changed=true;}changed|=ImGui::InputFloat3("Position",&value.position.x);changed|=ImGui::InputFloat3("Direction",&value.direction.x);changed|=ImGui::ColorEdit3("Colour",&value.colour.x);changed|=ImGui::InputFloat("Intensity",&value.intensity);changed|=ImGui::InputFloat("Range",&value.range);if(changed)sceneDirty=true;}
			}
			else ImGui::TextUnformatted("Select a pipeline pass or scene item."); ImGui::End();
			ImGui::Begin("Diagnostics"); if (openDocument) { auto diagnostics=openDocument->validate(); if(openScene)diagnostics.append(openScene->validate()); ImGui::Text("%zu error(s), %zu warning(s)",diagnostics.count(DiagnosticSeverity::Error),diagnostics.count(DiagnosticSeverity::Warning)); for(auto const&d:diagnostics.getDiagnostics()) ImGui::BulletText("[%s] %s",d.code.c_str(),d.message.c_str()); } else ImGui::TextColored(ImVec4(0.3f,1,0.4f,1),"No document loaded"); ImGui::End();
			ImGui::Begin("Allocations"); if(openDocument&&openDocument->graph){auto plan=openDocument->graph->buildAllocationPlan({1280,720});if(plan.valid){ImGui::Text("Physical estimate: %.2f MiB",plan.estimatedPhysicalBytes/1048576.0);for(auto const& image:plan.allocatedImages)ImGui::BulletText("%s.v%u -> allocation %u, %.1f KiB, passes %u-%u",image.debugName.c_str(),image.image.version,image.physicalAllocation,image.estimatedBytes/1024.0,image.firstPass,image.lastPass);}else ImGui::TextUnformatted("Allocation unavailable while graph is invalid.");} ImGui::End();
			ImGui::Begin("Viewport");if(previewStale){ImGui::TextColored(ImVec4(1.0f,0.65f,0.15f,1.0f),"STALE PREVIEW");ImGui::TextWrapped("%s",previewFailure.c_str());}else if(!previewFailure.empty())ImGui::TextColored(ImVec4(1.0f,0.3f,0.3f,1.0f),"Preview rebuild failed: %s",previewFailure.c_str());ImGui::TextUnformatted("PBR preview target will be presented here."); ImGui::End();
			ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 24)); ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x,24)); ImGui::Begin("##Status",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings); ImGui::Text("%s%s | %.1f FPS | %d submitted triangles | %llu known unique | Preview: %s",openDocument?"Loaded":"Ready",(pipelineDirty||sceneDirty)?" *":"",fps,renderSystem.getCurrentRenderInfo().trianglesRendered,(unsigned long long)(openScene?openScene->getKnownTriangleCount():0),openDocument?(previewStale?"stale last-valid generation":"current generation"):"no document");if(openScene&&openScene->getUnknownTriangleModelCount()&&ImGui::IsItemHovered())ImGui::SetTooltip("%zu visible .mppmodel source(s) are not included until model metadata is loaded.",openScene->getUnknownTriangleModelCount()); ImGui::End();
			ImGui::Render(); provider->setDrawData(ImGui::GetDrawData());
			renderSystem.startStatsCollection(); renderSystem.renderScene(scene, camera, glm::vec2(0), activePipeline); renderer.render(&renderSystem); renderSystem.finishStatsCollection(); window.show();
		}
		if(!activeGraphResource.empty()){renderSystem.removeRenderPipeline(activePipeline);resources.deleteResource(activeGraphResource);}
		scene->unload(); imGuiShutdown(&backend); renderSystem.destroyCoreResources(); window.destroy(); SDL_Quit();
		return 0;
	}
	catch (std::exception const& error) { MessageBoxA(nullptr, error.what(), "PipelineEditor Error", MB_ICONERROR); return 1; }
}
