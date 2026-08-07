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
#include "mpp/RenderGraphTargets.h"
#include "mpp/RenderPipeline.h"
#include "mpp/RenderTexture.h"
#include "mpp/ResourceManager.h"
#include "mpp/Scene.h"
#include "mpp/SceneRuntime.h"
#include "mpp/app/CommandStack.h"
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

namespace
{
	std::shared_ptr<PbrPipelineDocument> clonePipeline(std::shared_ptr<PbrPipelineDocument> const& value){if(!value)return {};auto result=std::make_shared<PbrPipelineDocument>(*value);if(value->graph)result->graph=std::make_shared<RenderGraph>(*value->graph);return result;}
	class PipelineSnapshotCommand final:public mpp::app::EditorCommand
	{
		std::string mName;std::shared_ptr<PbrPipelineDocument>* mTarget;std::shared_ptr<PbrPipelineDocument> mBefore,mAfter;
	public:PipelineSnapshotCommand(std::string name,std::shared_ptr<PbrPipelineDocument>* target,std::shared_ptr<PbrPipelineDocument> before,std::shared_ptr<PbrPipelineDocument> after):mName(std::move(name)),mTarget(target),mBefore(std::move(before)),mAfter(std::move(after)){}std::string const& name()const override{return mName;}void execute()override{*mTarget=clonePipeline(mAfter);}void undo()override{*mTarget=clonePipeline(mBefore);}
	};
	class SceneSnapshotCommand final:public mpp::app::EditorCommand
	{
		std::string mName;std::shared_ptr<SceneDocument>* mTarget;std::shared_ptr<SceneDocument> mBefore,mAfter;
		static std::shared_ptr<SceneDocument> clone(std::shared_ptr<SceneDocument> const& value){return value?std::make_shared<SceneDocument>(*value):nullptr;}
	public:SceneSnapshotCommand(std::string name,std::shared_ptr<SceneDocument>* target,std::shared_ptr<SceneDocument> before,std::shared_ptr<SceneDocument> after):mName(std::move(name)),mTarget(target),mBefore(std::move(before)),mAfter(std::move(after)){}std::string const& name()const override{return mName;}void execute()override{*mTarget=clone(mAfter);}void undo()override{*mTarget=clone(mBefore);}
	};
}

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
		int windowWidth=1440,windowHeight=900;float recoverySeconds=30.0f;{std::ifstream config("PipelineEditor.cfg");std::string key;while(std::getline(config,key,'=')){std::string value;if(!std::getline(config,value))break;if(key=="width")windowWidth=std::max(640,std::stoi(value));else if(key=="height")windowHeight=std::max(480,std::stoi(value));else if(key=="recoverySeconds")recoverySeconds=std::max(5.0f,std::stof(value));}}
		std::string startupPath;for(int argument=1;argument<__argc;++argument){std::string value=__argv[argument];if(value=="--width"&&argument+1<__argc)windowWidth=std::max(640,std::stoi(__argv[++argument]));else if(value=="--height"&&argument+1<__argc)windowHeight=std::max(480,std::stoi(__argv[++argument]));else if(value=="--recovery-seconds"&&argument+1<__argc)recoverySeconds=std::max(5.0f,std::stof(__argv[++argument]));else if(!value.starts_with("--"))startupPath=value;}
		std::shared_ptr<PbrPipelineDocument> openDocument;
		std::shared_ptr<SceneDocument> openScene;
		std::string currentPath, scenePath;
		std::vector<std::string> recentPaths;{std::ifstream recent("PipelineEditor.recent.txt");std::string path;while(std::getline(recent,path))if(!path.empty())recentPaths.push_back(path);}
		auto rememberRecent=[&](std::string const& path){recentPaths.erase(std::remove(recentPaths.begin(),recentPaths.end(),path),recentPaths.end());recentPaths.insert(recentPaths.begin(),path);if(recentPaths.size()>8)recentPaths.resize(8);std::ofstream recent("PipelineEditor.recent.txt",std::ios::trunc);for(auto const& value:recentPaths)recent<<value<<'\n';};
		bool recoveredDocument = false;
		if (!startupPath.empty())
		{
			currentPath = startupPath; rememberRecent(currentPath);
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
		WindowSDL window("PBR Pipeline Editor"); window.create(windowWidth, windowHeight, false, true);
		Logger logger; if (!logger.initialise("PipelineEditor.log", Logger::Level::Debug)) throw std::runtime_error("Could not create editor log.");
		RenderSystem renderSystem(window.getWidth(), window.getHeight(), &logger);
		ResourceManager resources(&renderSystem, &logger); renderSystem.createCoreResources(&resources);
		ImGuiBackendData backend{}; imGuiSetup(&renderSystem, &resources, &backend, true);
		InputManagerSDL input; TimerSDL timer; timer.reset();
		auto font = resources.getResource("__ImGui_Font__", true);
		auto provider = std::make_shared<ImGuiDataProvider>(std::vector<ResourcePtr>{ font });
		BufferRenderer renderer(provider);
		SceneRuntime sceneRuntime(&renderSystem,&resources);auto scene = std::make_shared<Scene>(&renderSystem); scene->load(); scene->setClearColour(Colour(0.094f, 0.106f, 0.125f));uint32_t viewportWidth=960,viewportHeight=720;scene->setViewport(0,0,viewportWidth,viewportHeight);
		auto camera = std::make_shared<Camera>(glm::vec3(0, 3, 8), 0.0f, 0.0f, 0.0f, 60.0f, float(windowWidth) / float(windowHeight));
		renderSystem.getOrCreateRenderPipeline("EditorUI");
		std::string activePipeline = "EditorUI", activeGraphResource, previewFailure;RenderTargetPtr activePreviewTarget;ImTextureID activePreviewTexture=0;bool previewStale=false,documentChangedSincePreview=false; uint32_t runtimeGeneration = 0;
		auto rebuildPreview = [&]()
		{
			if (!openDocument || !openDocument->graph || openDocument->validate().hasErrors()){previewStale=!activeGraphResource.empty();previewFailure="Working document is invalid; retaining the last valid preview.";return false;}
			auto suffix = std::to_string(++runtimeGeneration), candidatePipeline="EditorPreview."+suffix, candidateGraph="PipelineEditor.Graph."+suffix;bool graphDeclared=false,pipelineDeclared=false;
			try
			{
				auto graphStream=std::make_shared<RenderGraphStream>(&resources);graphStream->setGraph(openDocument->graph);auto graphResource=resources.declareResource(candidateGraph,graphStream).first;graphDeclared=true;graphResource->load();graphResource->create();RenderPipelineOptions previewOptions;previewOptions.mode=RenderPipelineMode::XmlGraphPbrForward;previewOptions.graphTemplate=graphResource;RenderTargetPtr candidatePreviewTarget;
				for(auto const& import:openDocument->imports){GraphImageDesc desc;desc.format=import.format;desc.usage=import.usage;desc.external=true;desc.transient=false;for(auto image:openDocument->graph->getImportedImages()){auto info=openDocument->graph->getImageInfo(image);if(info.importName==import.id||info.importName==import.semantic){desc=info.desc;break;}}auto width=desc.absoluteSize.x?desc.absoluteSize.x:std::max(1u,(uint32_t)(viewportWidth*desc.relativeSize.x));auto height=desc.absoluteSize.y?desc.absoluteSize.y:std::max(1u,(uint32_t)(viewportHeight*desc.relativeSize.y));auto target=renderSystem.createRenderTexture("PipelineEditor.Import."+import.id+"."+suffix,width,height,makeGraphRenderTextureOptions(desc));previewOptions.graphImports[import.id]=target;previewOptions.graphImports[import.semantic]=target;if(import.id=="screen"||import.semantic=="presentation")candidatePreviewTarget=target;}
				renderSystem.getOrCreateRenderPipeline(candidatePipeline,previewOptions);pipelineDeclared=true;
				if(openScene){if(!sceneRuntime.rebuild(*openScene)){std::string message="Preview scene rebuild failed.";for(auto const& diagnostic:sceneRuntime.getDiagnostics().getDiagnostics())if(diagnostic.severity==DiagnosticSeverity::Error){message+=" "+diagnostic.message;break;}throw std::runtime_error(message);}scene=sceneRuntime.getScene();scene->setClearColour(Colour(0.094f,0.106f,0.125f));scene->setViewport(0,0,viewportWidth,viewportHeight);camera->setLookAt(openScene->camera.position,openScene->camera.target);camera->setFov(openScene->camera.fov);camera->setClipDistances(openScene->camera.nearPlane,openScene->camera.farPlane);}
				auto obsoletePipeline=activePipeline,obsoleteGraph=activeGraphResource;auto obsoleteTexture=activePreviewTexture;activePipeline=candidatePipeline;activeGraphResource=candidateGraph;activePreviewTarget=candidatePreviewTarget;activePreviewTexture=0;if(auto texture=std::dynamic_pointer_cast<RenderTexture>(activePreviewTarget))activePreviewTexture=provider->registerTexture(texture);if(obsoleteTexture)provider->unregisterTexture(obsoleteTexture);previewStale=false;documentChangedSincePreview=false;previewFailure.clear();if(!obsoleteGraph.empty()){renderSystem.removeRenderPipeline(obsoletePipeline);resources.deleteResource(obsoleteGraph);}return true;
			}
			catch(std::exception const& error)
			{
				if(pipelineDeclared)renderSystem.removeRenderPipeline(candidatePipeline);if(graphDeclared){try{resources.deleteResource(candidateGraph);}catch(...){}}previewStale=!activeGraphResource.empty();previewFailure=error.what();return false;
			}
		};
		rebuildPreview();
		int selectedPass = -1, selectedImage = -1, selectedImport = -1, selectedModel = -1, selectedLocalResource = -1, selectedExternalResource = -1;mpp::app::CommandStack pipelineCommands(256),sceneCommands(256);bool lastEditScene=false;
		auto loadWorkspace = [&](std::string const& path)
		{
			openDocument = std::make_shared<PbrPipelineDocument>(resource_parsers::PbrPipelineDocumentLoader::fromFile(path));
			openScene.reset(); scenePath.clear(); currentPath = openDocument->importedFromRenderGraph ? std::string() : path; rememberRecent(path); selectedPass = -1; selectedImage = -1; selectedImport = -1; selectedModel = -1; selectedLocalResource = -1; selectedExternalResource = -1;pipelineCommands.clear();sceneCommands.clear();
			if (!openDocument->previewScene.empty()) { scenePath=(std::filesystem::path(path).parent_path()/openDocument->previewScene).string(); openScene = std::make_shared<SceneDocument>(resource_parsers::SceneParser::fromFile(scenePath)); }
		};
		bool running = true, pipelineDirty = recoveredDocument, sceneDirty = false, resetLayout = true, showPreferences = false; float fps = 0, fpsTime = 0, recoveryTimer = 0; int frames = 0;
		while (running)
		{
			float dt = timer.getDeltaTime(); fpsTime += dt; recoveryTimer += dt; ++frames; if (fpsTime >= 0.5f) { fps = frames / fpsTime; frames = 0; fpsTime = 0; }
			if (pipelineDirty && openDocument && !currentPath.empty() && recoveryTimer >= recoverySeconds) { resource_parsers::PbrPipelineSerializer::toFile(*openDocument, currentPath + ".recovery"); recoveryTimer = 0; }
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
			bool requestNew=false, requestOpen=false, requestSave=false, requestSaveScene=false, requestUndo=false, requestRedo=false, requestDuplicate=false, requestDelete=false, requestAutoOrder=false;std::string requestedRecent;
			if (ImGui::BeginMainMenuBar()) { if (ImGui::BeginMenu("File")) { if(ImGui::MenuItem("New", "Ctrl+N"))requestNew=true; if(ImGui::MenuItem("Open...", "Ctrl+O"))requestOpen=true; if(!recentPaths.empty()&&ImGui::BeginMenu("Open Recent")){for(auto const& path:recentPaths)if(ImGui::MenuItem(path.c_str()))requestedRecent=path;ImGui::EndMenu();} if(ImGui::MenuItem("Save", "Ctrl+S",false,openDocument!=nullptr))requestSave=true; if(ImGui::MenuItem("Save Scene",nullptr,false,openScene!=nullptr))requestSaveScene=true; ImGui::Separator(); if (ImGui::MenuItem("Exit")) { if(!(pipelineDirty||sceneDirty)||MessageBoxA(nullptr,"Discard unsaved document changes?","PipelineEditor",MB_YESNO|MB_ICONWARNING)==IDYES)running=false; } ImGui::EndMenu(); } if(ImGui::BeginMenu("Edit")){auto& commands=lastEditScene?sceneCommands:pipelineCommands;if(ImGui::MenuItem("Undo","Ctrl+Z",false,commands.canUndo()))requestUndo=true;if(ImGui::MenuItem("Redo","Ctrl+Y",false,commands.canRedo()))requestRedo=true;ImGui::Separator();if(ImGui::MenuItem("Preferences..."))showPreferences=true;ImGui::EndMenu();} if(ImGui::BeginMenu("Pipeline")){ImGui::MenuItem("Validate");ImGui::MenuItem("Apply/Rebuild");if(ImGui::MenuItem("Auto-order Pass Dependencies",nullptr,false,openDocument&&openDocument->graph))requestAutoOrder=true;ImGui::EndMenu();} if(ImGui::BeginMenu("Window")){if(ImGui::MenuItem("Reset Layout"))resetLayout=true;ImGui::EndMenu();} ImGui::EndMainMenuBar(); }
			ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse); if(ImGui::Button("New"))requestNew=true; ImGui::SameLine(); if(ImGui::Button("Open"))requestOpen=true; ImGui::SameLine(); if(ImGui::Button("Save All")&&openDocument){requestSave=true;requestSaveScene=openScene!=nullptr;} ImGui::SameLine(); if(ImGui::Button("Undo"))requestUndo=true; ImGui::SameLine(); if(ImGui::Button("Redo"))requestRedo=true; ImGui::SameLine(); ImGui::Button("Add Pass"); ImGui::SameLine(); if(ImGui::Button("Duplicate"))requestDuplicate=true; ImGui::SameLine(); if(ImGui::Button("Delete"))requestDelete=true; ImGui::SameLine(); ImGui::Button("Validate"); ImGui::SameLine(); if(ImGui::Button("Apply/Rebuild"))rebuildPreview(); ImGui::End();
			if(requestAutoOrder&&openDocument&&openDocument->graph){auto result=openDocument->graph->buildDependencyOrder();if(result.valid){std::vector<GraphPassHandle> order=result.passOrder;for(uint32_t pass=0;pass<openDocument->graph->getPassCount();++pass)if(std::none_of(order.begin(),order.end(),[&](auto handle){return handle.id==pass;}))order.push_back({pass});auto before=clonePipeline(openDocument);openDocument->graph->reorderPasses(order);auto after=clonePipeline(openDocument);documentChangedSincePreview=true;pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>("Auto-order Passes",&openDocument,before,after));selectedPass=-1;lastEditScene=false;pipelineDirty=currentPath.empty()||pipelineCommands.dirty();}else previewFailure=result.diagnostics.empty()?"Pass dependencies cannot be ordered.":result.diagnostics.front();}
			if(requestUndo){auto& commands=lastEditScene?sceneCommands:pipelineCommands;if(commands.undo()){documentChangedSincePreview=true;if(lastEditScene)sceneDirty=scenePath.empty()||commands.dirty();else pipelineDirty=currentPath.empty()||commands.dirty();}}
			if(requestRedo){auto& commands=lastEditScene?sceneCommands:pipelineCommands;if(commands.redo()){documentChangedSincePreview=true;if(lastEditScene)sceneDirty=scenePath.empty()||commands.dirty();else pipelineDirty=currentPath.empty()||commands.dirty();}}
			if(requestDuplicate&&openScene&&selectedModel>=0&&(size_t)selectedModel<openScene->models.size()){auto before=std::make_shared<SceneDocument>(*openScene);auto value=openScene->models[(size_t)selectedModel];auto base=value.id+".Copy";value.id=base;unsigned suffix=2;auto exists=[&](std::string const& id){return std::any_of(openScene->models.begin(),openScene->models.end(),[&](auto const& current){return current.id==id;});};while(exists(value.id))value.id=base+std::to_string(suffix++);openScene->models.insert(openScene->models.begin()+selectedModel+1,value);++selectedModel;auto after=std::make_shared<SceneDocument>(*openScene);documentChangedSincePreview=true;sceneCommands.execute(std::make_unique<SceneSnapshotCommand>("Duplicate Scene Model",&openScene,before,after));lastEditScene=true;sceneDirty=scenePath.empty()||sceneCommands.dirty();}
			if(requestDuplicate&&openScene&&selectedModel<=-100){auto index=(size_t)(-100-selectedModel);if(index<openScene->lights.size()){auto before=std::make_shared<SceneDocument>(*openScene);auto value=openScene->lights[index];auto base=value.id+".Copy";value.id=base;unsigned suffix=2;while(std::any_of(openScene->lights.begin(),openScene->lights.end(),[&](auto const& current){return current.id==value.id;}))value.id=base+std::to_string(suffix++);openScene->lights.insert(openScene->lights.begin()+index+1,value);selectedModel=-100-(int)(index+1);auto after=std::make_shared<SceneDocument>(*openScene);documentChangedSincePreview=true;sceneCommands.execute(std::make_unique<SceneSnapshotCommand>("Duplicate Scene Light",&openScene,before,after));lastEditScene=true;sceneDirty=scenePath.empty()||sceneCommands.dirty();}}
			if(requestDelete&&openScene&&selectedModel>=0&&(size_t)selectedModel<openScene->models.size()){auto before=std::make_shared<SceneDocument>(*openScene);openScene->models.erase(openScene->models.begin()+selectedModel);selectedModel=-1;auto after=std::make_shared<SceneDocument>(*openScene);documentChangedSincePreview=true;sceneCommands.execute(std::make_unique<SceneSnapshotCommand>("Delete Scene Model",&openScene,before,after));lastEditScene=true;sceneDirty=scenePath.empty()||sceneCommands.dirty();}
			if(requestDelete&&openScene&&selectedModel<=-100){auto index=(size_t)(-100-selectedModel);if(index<openScene->lights.size()){auto before=std::make_shared<SceneDocument>(*openScene);openScene->lights.erase(openScene->lights.begin()+index);selectedModel=-1;auto after=std::make_shared<SceneDocument>(*openScene);documentChangedSincePreview=true;sceneCommands.execute(std::make_unique<SceneSnapshotCommand>("Delete Scene Light",&openScene,before,after));lastEditScene=true;sceneDirty=scenePath.empty()||sceneCommands.dirty();}}
			if(requestNew&&(!(pipelineDirty||sceneDirty)||MessageBoxA(nullptr,"Discard unsaved document changes?","PipelineEditor",MB_YESNO|MB_ICONWARNING)==IDYES)){loadWorkspace("resources/FullPbrPipeline.xml");currentPath.clear();pipelineDirty=true;rebuildPreview();}
			if(requestOpen&&(!(pipelineDirty||sceneDirty)||MessageBoxA(nullptr,"Discard unsaved document changes?","PipelineEditor",MB_YESNO|MB_ICONWARNING)==IDYES)){if(auto path=mpp::app::openXmlFileDialog(window.getWindow(),"Open PBR Pipeline")){loadWorkspace(*path);pipelineDirty=false;sceneDirty=false;rebuildPreview();}}
			if(!requestedRecent.empty()&&(!(pipelineDirty||sceneDirty)||MessageBoxA(nullptr,"Discard unsaved document changes?","PipelineEditor",MB_YESNO|MB_ICONWARNING)==IDYES)){if(std::filesystem::exists(requestedRecent)){loadWorkspace(requestedRecent);pipelineDirty=false;sceneDirty=false;rebuildPreview();}else{recentPaths.erase(std::remove(recentPaths.begin(),recentPaths.end(),requestedRecent),recentPaths.end());std::ofstream recent("PipelineEditor.recent.txt",std::ios::trunc);for(auto const& value:recentPaths)recent<<value<<'\n';MessageBoxA(nullptr,"The recent pipeline no longer exists and was removed from the list.","PipelineEditor",MB_OK|MB_ICONWARNING);}}
			if(requestSaveScene&&openScene){if(scenePath.empty()){auto path=mpp::app::saveXmlFileDialog(window.getWindow(),"Save Preview Scene","preview.scene.xml");if(path)scenePath=*path;}if(!scenePath.empty()){resource_parsers::SceneSerializer::toFile(*openScene,scenePath);sceneCommands.markSavePoint();sceneDirty=false;}}
			if(requestSave&&openDocument){if(currentPath.empty()){auto path=mpp::app::saveXmlFileDialog(window.getWindow(),"Save PBR Pipeline","pipeline.xml");if(path)currentPath=*path;}if(!currentPath.empty()){resource_parsers::PbrPipelineSerializer::toFile(*openDocument,currentPath);rememberRecent(currentPath);pipelineCommands.markSavePoint();pipelineDirty=false;std::filesystem::remove(currentPath+".recovery");}}
			if(showPreferences){ImGui::Begin("Preferences",&showPreferences);ImGui::InputInt("Startup width",&windowWidth);ImGui::InputInt("Startup height",&windowHeight);ImGui::InputFloat("Recovery interval (seconds)",&recoverySeconds);windowWidth=std::max(640,windowWidth);windowHeight=std::max(480,windowHeight);recoverySeconds=std::max(5.0f,recoverySeconds);if(ImGui::Button("Save Preferences")){std::ofstream config("PipelineEditor.cfg",std::ios::trunc);config<<"width="<<windowWidth<<'\n'<<"height="<<windowHeight<<'\n'<<"recoverySeconds="<<recoverySeconds<<'\n';}ImGui::TextDisabled("Window dimensions apply on next launch.");ImGui::End();}
			ImGui::Begin("Pipeline Hierarchy"); ImGui::TextUnformatted(openDocument ? openDocument->name.c_str() : "Pipeline");
			if (openDocument && openDocument->graph && ImGui::TreeNodeEx("Passes", ImGuiTreeNodeFlags_DefaultOpen)) { for (uint32_t pass=0; pass<openDocument->graph->getPassCount(); ++pass) { auto info=openDocument->graph->getPassInfo({pass}); if(ImGui::Selectable(info.name.c_str(),selectedPass==(int)pass)){selectedPass=(int)pass;selectedImage=-1;selectedImport=-1;selectedModel=-1;selectedLocalResource=-1;selectedExternalResource=-1;} } ImGui::TreePop(); }
			if(openDocument&&openDocument->graph&&ImGui::TreeNodeEx("Images",ImGuiTreeNodeFlags_DefaultOpen)){for(uint32_t image=0;image<openDocument->graph->getImageCount();++image){auto info=openDocument->graph->getImageInfo({image,0});if(ImGui::Selectable(info.name.c_str(),selectedImage==(int)image)){selectedImage=(int)image;selectedImport=-1;selectedPass=-1;selectedModel=-1;selectedLocalResource=-1;selectedExternalResource=-1;}}ImGui::TreePop();}
			if(openDocument&&!openDocument->imports.empty()&&ImGui::TreeNodeEx("Typed Imports",ImGuiTreeNodeFlags_DefaultOpen)){for(size_t index=0;index<openDocument->imports.size();++index)if(ImGui::Selectable(openDocument->imports[index].id.c_str(),selectedImport==(int)index)){selectedImport=(int)index;selectedImage=-1;selectedPass=-1;selectedModel=-1;selectedLocalResource=-1;selectedExternalResource=-1;}ImGui::TreePop();}
			if(openDocument&&!openDocument->localResources.empty()&&ImGui::TreeNodeEx("Local Resources",ImGuiTreeNodeFlags_DefaultOpen)){for(size_t index=0;index<openDocument->localResources.size();++index)if(ImGui::Selectable(openDocument->localResources[index].name.c_str(),selectedLocalResource==(int)index)){selectedLocalResource=(int)index;selectedExternalResource=-1;selectedImage=-1;selectedImport=-1;selectedPass=-1;selectedModel=-1;}ImGui::TreePop();}
			if(openDocument&&!openDocument->externalResources.empty()&&ImGui::TreeNodeEx("External Libraries",ImGuiTreeNodeFlags_DefaultOpen)){for(size_t index=0;index<openDocument->externalResources.size();++index){auto const& value=openDocument->externalResources[index];auto label=value.libraryName+"::"+value.resource.name;if(ImGui::Selectable(label.c_str(),selectedExternalResource==(int)index)){selectedExternalResource=(int)index;selectedLocalResource=-1;selectedImage=-1;selectedImport=-1;selectedPass=-1;selectedModel=-1;}}ImGui::TreePop();}
			if (openScene && ImGui::TreeNodeEx("Preview Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
				if(ImGui::Selectable("Camera",selectedModel==-2)){selectedModel=-2;selectedPass=-1;selectedImage=-1;selectedImport=-1;selectedLocalResource=-1;selectedExternalResource=-1;}
				if(ImGui::Selectable("Environment",selectedModel==-3)){selectedModel=-3;selectedPass=-1;selectedImage=-1;selectedImport=-1;selectedLocalResource=-1;selectedExternalResource=-1;}
				if(ImGui::TreeNodeEx("Models",ImGuiTreeNodeFlags_DefaultOpen)){for(size_t model=0;model<openScene->models.size();++model)if(ImGui::Selectable(openScene->models[model].id.c_str(),selectedModel==(int)model)){selectedModel=(int)model;selectedPass=-1;selectedImage=-1;selectedImport=-1;selectedLocalResource=-1;selectedExternalResource=-1;}ImGui::TreePop();}
				if(ImGui::TreeNodeEx("Lights",ImGuiTreeNodeFlags_DefaultOpen)){for(size_t light=0;light<openScene->lights.size();++light)if(ImGui::Selectable(openScene->lights[light].id.c_str(),selectedModel==-100-(int)light)){selectedModel=-100-(int)light;selectedPass=-1;selectedImage=-1;selectedImport=-1;selectedLocalResource=-1;selectedExternalResource=-1;}ImGui::TreePop();}
				ImGui::TreePop(); } ImGui::End();
			ImGui::Begin("Inspector");
			if(openDocument&&openDocument->graph&&selectedPass>=0)
			{
				auto info=openDocument->graph->getPassInfo({(uint32_t)selectedPass}); ImGui::Text("Pass: %s",info.name.c_str()); ImGui::Text("Factory: %s",info.callbackFactory.c_str());
				bool enabled=info.enabled; if(ImGui::Checkbox("Enabled",&enabled)){auto before=clonePipeline(openDocument);openDocument->graph->setPassEnabled({(uint32_t)selectedPass},enabled);auto after=clonePipeline(openDocument);documentChangedSincePreview=true;pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>("Toggle Pass",&openDocument,before,after));lastEditScene=false;pipelineDirty=currentPath.empty()||pipelineCommands.dirty();}
				ImGui::Text("Inputs: %zu  Colour outputs: %zu  Depth outputs: %zu",info.sampledInputs.size(),info.colourOutputs.size(),info.depthOutputs.size());
				if(ImGui::CollapsingHeader("Raster State"))
				{
					auto raster=info.rasterState;bool changed=ImGui::Checkbox("Explicit state",&raster.explicitState);int fill=(int)raster.fillMode,front=(int)raster.frontFace,cull=(int)raster.cullMode,depth=(int)raster.depthCompare;if(ImGui::Combo("Fill mode",&fill,"Fill\0Line\0")){raster.fillMode=(GraphFillMode)fill;changed=true;}if(ImGui::Combo("Front face",&front,"Counter-clockwise\0Clockwise\0")){raster.frontFace=(GraphFrontFace)front;changed=true;}if(ImGui::Combo("Cull mode",&cull,"None\0Front\0Back\0")){raster.cullMode=(GraphCullMode)cull;changed=true;}changed|=ImGui::Checkbox("Depth test",&raster.depthTest);changed|=ImGui::Checkbox("Depth write",&raster.depthWrite);if(ImGui::Combo("Depth comparison",&depth,"Never\0Less\0Equal\0Less or equal\0Greater\0Not equal\0Greater or equal\0Always\0")){raster.depthCompare=(GraphCompareOp)depth;changed=true;}changed|=ImGui::Checkbox("Blend",&raster.blend);changed|=ImGui::Checkbox("Multisample",&raster.multisample);changed|=ImGui::Checkbox("Alpha to coverage",&raster.alphaToCoverage);changed|=ImGui::Checkbox("Scissor",&raster.scissor);if(changed){auto before=clonePipeline(openDocument);openDocument->graph->setPassRasterState({(uint32_t)selectedPass},raster);auto after=clonePipeline(openDocument);documentChangedSincePreview=true;pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>("Edit Raster State",&openDocument,before,after));lastEditScene=false;pipelineDirty=currentPath.empty()||pipelineCommands.dirty();}
				}
				if(ImGui::CollapsingHeader("Attachments")){for(size_t index=0;index<info.colourOutputs.size();++index){auto const& output=info.colourOutputs[index];auto image=openDocument->graph->getImageInfo(output.image);ImGui::BulletText("Colour %zu: %s mip %u",index,image.name.c_str(),output.mipLevel);}for(auto const& output:info.depthOutputs){auto image=openDocument->graph->getImageInfo(output.image);ImGui::BulletText("Depth: %s mip %u",image.name.c_str(),output.mipLevel);}}
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
					if(changed){auto before=clonePipeline(openDocument);openDocument->graph->setPassParameters({(uint32_t)selectedPass},info.parameters);auto after=clonePipeline(openDocument);documentChangedSincePreview=true;pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>("Edit Pass Parameters",&openDocument,before,after));lastEditScene=false;pipelineDirty=currentPath.empty()||pipelineCommands.dirty();}
				}
			}
			else if(openDocument&&openDocument->graph&&selectedImage>=0&&(size_t)selectedImage<openDocument->graph->getImageCount())
			{
				auto handle=GraphImageHandle{(uint32_t)selectedImage,0};auto info=openDocument->graph->getImageInfo(handle);ImGui::Text("Image: %s",info.name.c_str());ImGui::Text("Format: %s",graphImageFormatName(info.desc.format));ImGui::Text("Usage flags: 0x%X",(unsigned)info.desc.usage);if(!info.importName.empty())ImGui::Text("Import: %s",info.importName.c_str());bool changed=false;changed|=ImGui::InputFloat2("Relative size",&info.desc.relativeSize.x);int samples=(int)info.desc.samples,mips=(int)info.desc.mipLevels;if(ImGui::InputInt("Samples",&samples)){info.desc.samples=(uint32_t)std::max(0,samples);changed=true;}if(ImGui::InputInt("Mip levels",&mips)){info.desc.mipLevels=(uint32_t)std::max(0,mips);changed=true;}changed|=ImGui::Checkbox("External",&info.desc.external);changed|=ImGui::Checkbox("Transient",&info.desc.transient);if(changed){auto before=clonePipeline(openDocument);try{openDocument->graph->setImageDesc(handle,info.desc);auto after=clonePipeline(openDocument);documentChangedSincePreview=true;pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>("Edit Graph Image",&openDocument,before,after));lastEditScene=false;pipelineDirty=currentPath.empty()||pipelineCommands.dirty();}catch(std::exception const& error){previewFailure=error.what();}}
			}
			else if(openDocument&&selectedImport>=0&&(size_t)selectedImport<openDocument->imports.size())
			{
				auto before=clonePipeline(openDocument);auto& value=openDocument->imports[(size_t)selectedImport];ImGui::Text("Import: %s",value.id.c_str());ImGui::Text("Format: %s",graphImageFormatName(value.format));ImGui::Text("Usage flags: 0x%X",(unsigned)value.usage);bool changed=ImGui::Checkbox("Required",&value.required);char semantic[256]{},fallback[256]{};strncpy_s(semantic,value.semantic.c_str(),255);strncpy_s(fallback,value.fallback.c_str(),255);if(ImGui::InputText("Semantic",semantic,sizeof(semantic))){value.semantic=semantic;changed=true;}if(ImGui::InputText("Fallback",fallback,sizeof(fallback))){value.fallback=fallback;changed=true;}if(changed){auto after=clonePipeline(openDocument);documentChangedSincePreview=true;pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>("Edit Typed Import",&openDocument,before,after));lastEditScene=false;pipelineDirty=currentPath.empty()||pipelineCommands.dirty();}
			}
			else if(openDocument&&selectedLocalResource>=0&&(size_t)selectedLocalResource<openDocument->localResources.size())
			{
				auto const& value=openDocument->localResources[(size_t)selectedLocalResource];ImGui::Text("Local resource: %s",value.name.c_str());ImGui::Text("Type: %s",value.definition.getName().c_str());ImGui::TextUnformatted("Editable pipeline-owned resource");
			}
			else if(openDocument&&selectedExternalResource>=0&&(size_t)selectedExternalResource<openDocument->externalResources.size())
			{
				auto const& value=openDocument->externalResources[(size_t)selectedExternalResource];auto qualified=value.libraryName+"::"+value.resource.name;ImGui::Text("External resource: %s",qualified.c_str());ImGui::Text("Library: %s",value.libraryPath.c_str());ImGui::TextDisabled("Read-only");if(ImGui::Button("Make Local Copy")){auto localName=value.resource.name+".Local";unsigned suffix=2;while(std::any_of(openDocument->localResources.begin(),openDocument->localResources.end(),[&](auto const& current){return current.name==localName;}))localName=value.resource.name+".Local"+std::to_string(suffix++);auto before=clonePipeline(openDocument);if(openDocument->makeLocalCopy(qualified,localName)){auto after=clonePipeline(openDocument);documentChangedSincePreview=true;pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>("Make Local Copy",&openDocument,before,after));selectedLocalResource=(int)openDocument->localResources.size()-1;selectedExternalResource=-1;lastEditScene=false;pipelineDirty=currentPath.empty()||pipelineCommands.dirty();}}
			}
			else if(openScene&&selectedModel>=0)
			{
				auto before=std::make_shared<SceneDocument>(*openScene);auto& model=openScene->models[(size_t)selectedModel]; ImGui::Text("Model: %s",model.id.c_str()); bool changed=false;
				changed|=ImGui::InputFloat3("Translation",&model.translation.x); changed|=ImGui::InputFloat3("Rotation (degrees)",&model.rotationDegrees.x); changed|=ImGui::InputFloat3("Scale",&model.scale.x);
				if(model.source==SceneModelSource::Box){changed|=ImGui::InputFloat("Width",&model.primitive.width);changed|=ImGui::InputFloat("Height",&model.primitive.height);changed|=ImGui::InputFloat("Depth",&model.primitive.depth);}else if(model.source==SceneModelSource::Sphere){changed|=ImGui::InputFloat("Radius",&model.primitive.radius);int resolution=(int)model.primitive.resolution;if(ImGui::InputInt("Resolution",&resolution)){model.primitive.resolution=(uint32_t)std::max(0,resolution);changed=true;}}else if(model.source==SceneModelSource::Cylinder){changed|=ImGui::InputFloat("Length",&model.primitive.height);changed|=ImGui::InputFloat("Bottom radius",&model.primitive.radius);changed|=ImGui::InputFloat("Top radius",&model.primitive.topRadius);int resolution=(int)model.primitive.resolution;if(ImGui::InputInt("Resolution",&resolution)){model.primitive.resolution=(uint32_t)std::max(0,resolution);changed=true;}}else if(model.source==SceneModelSource::Grid){changed|=ImGui::InputFloat("Width",&model.primitive.width);changed|=ImGui::InputFloat("Depth",&model.primitive.depth);int x=(int)model.primitive.segmentsX,z=(int)model.primitive.segmentsZ;if(ImGui::InputInt("X segments",&x)){model.primitive.segmentsX=(uint32_t)std::max(0,x);changed=true;}if(ImGui::InputInt("Z segments",&z)){model.primitive.segmentsZ=(uint32_t)std::max(0,z);changed=true;}changed|=ImGui::InputFloat("Texture repeat U",&model.primitive.textureRepeatU);changed|=ImGui::InputFloat("Texture repeat V",&model.primitive.textureRepeatV);}
				changed|=ImGui::Checkbox("Visible",&model.visible); changed|=ImGui::Checkbox("Shadow caster",&model.shadowCaster);
				char binding[256]{}; strncpy_s(binding,model.materialBinding.c_str(),255); if(ImGui::InputText("Material binding",binding,sizeof(binding))){model.materialBinding=binding;changed=true;}
				char layers[256]{}; std::string layerText; for(auto const& layer:model.layers){if(!layerText.empty())layerText+=",";layerText+=layer;} strncpy_s(layers,layerText.c_str(),255); if(ImGui::InputText("Layers (comma separated)",layers,sizeof(layers))){model.layers.clear();std::stringstream stream(layers);std::string layer;while(std::getline(stream,layer,','))if(!layer.empty())model.layers.push_back(layer);changed=true;}
				if(changed){auto after=std::make_shared<SceneDocument>(*openScene);documentChangedSincePreview=true;sceneCommands.execute(std::make_unique<SceneSnapshotCommand>("Edit Scene Model",&openScene,before,after));lastEditScene=true;sceneDirty=scenePath.empty()||sceneCommands.dirty();}
			}
			else if(openScene&&selectedModel==-2)
			{
				auto before=std::make_shared<SceneDocument>(*openScene);auto& value=openScene->camera; ImGui::TextUnformatted("Camera"); bool changed=false; changed|=ImGui::InputFloat3("Position",&value.position.x);changed|=ImGui::InputFloat3("Target",&value.target.x);changed|=ImGui::InputFloat("Vertical FOV",&value.fov);changed|=ImGui::InputFloat("Near plane",&value.nearPlane);changed|=ImGui::InputFloat("Far plane",&value.farPlane);if(changed){auto after=std::make_shared<SceneDocument>(*openScene);documentChangedSincePreview=true;sceneCommands.execute(std::make_unique<SceneSnapshotCommand>("Edit Scene Camera",&openScene,before,after));lastEditScene=true;sceneDirty=scenePath.empty()||sceneCommands.dirty();}
			}
			else if(openScene&&selectedModel==-3)
			{
				char value[256]{};strncpy_s(value,openScene->environmentBinding.c_str(),255);if(ImGui::InputText("Environment binding",value,sizeof(value))){auto before=std::make_shared<SceneDocument>(*openScene);openScene->environmentBinding=value;auto after=std::make_shared<SceneDocument>(*openScene);documentChangedSincePreview=true;sceneCommands.execute(std::make_unique<SceneSnapshotCommand>("Edit Environment Binding",&openScene,before,after));lastEditScene=true;sceneDirty=scenePath.empty()||sceneCommands.dirty();}
			}
			else if(openScene&&selectedModel<=-100)
			{
				auto index=(size_t)(-100-selectedModel);if(index<openScene->lights.size()){auto before=std::make_shared<SceneDocument>(*openScene);auto& value=openScene->lights[index];ImGui::Text("Light: %s",value.id.c_str());bool changed=false;int type=value.type==SceneLightType::Point?1:0;if(ImGui::Combo("Type",&type,"Directional\0Point\0")){value.type=type?SceneLightType::Point:SceneLightType::Directional;changed=true;}changed|=ImGui::InputFloat3("Position",&value.position.x);changed|=ImGui::InputFloat3("Direction",&value.direction.x);changed|=ImGui::ColorEdit3("Colour",&value.colour.x);changed|=ImGui::InputFloat("Intensity",&value.intensity);changed|=ImGui::InputFloat("Range",&value.range);if(changed){auto after=std::make_shared<SceneDocument>(*openScene);documentChangedSincePreview=true;sceneCommands.execute(std::make_unique<SceneSnapshotCommand>("Edit Scene Light",&openScene,before,after));lastEditScene=true;sceneDirty=scenePath.empty()||sceneCommands.dirty();}}
			}
			else ImGui::TextUnformatted("Select a pipeline pass or scene item."); ImGui::End();
			ImGui::Begin("Diagnostics"); if (openDocument) { auto diagnostics=openDocument->validate(); if(openScene)diagnostics.append(openScene->validate());diagnostics.append(sceneRuntime.getDiagnostics()); ImGui::Text("%zu error(s), %zu warning(s)",diagnostics.count(DiagnosticSeverity::Error),diagnostics.count(DiagnosticSeverity::Warning)); for(auto const&d:diagnostics.getDiagnostics()) ImGui::BulletText("[%s] %s",d.code.c_str(),d.message.c_str()); } else ImGui::TextColored(ImVec4(0.3f,1,0.4f,1),"No document loaded"); ImGui::End();
			ImGui::Begin("Allocations"); if(openDocument&&openDocument->graph){auto plan=openDocument->graph->buildAllocationPlan({1280,720});if(plan.valid){ImGui::Text("Physical estimate: %.2f MiB",plan.estimatedPhysicalBytes/1048576.0);for(auto const& image:plan.allocatedImages)ImGui::BulletText("%s.v%u -> allocation %u, %.1f KiB, passes %u-%u",image.debugName.c_str(),image.image.version,image.physicalAllocation,image.estimatedBytes/1024.0,image.firstPass,image.lastPass);}else ImGui::TextUnformatted("Allocation unavailable while graph is invalid.");} ImGui::End();
			if(documentChangedSincePreview&&!activeGraphResource.empty()){previewStale=true;if(previewFailure.empty())previewFailure="Working changes have not been applied; showing the last valid generation.";}
			ImGui::Begin("Viewport");if(previewStale){ImGui::TextColored(ImVec4(1.0f,0.65f,0.15f,1.0f),"STALE PREVIEW");ImGui::TextWrapped("%s",previewFailure.c_str());}else if(!previewFailure.empty())ImGui::TextColored(ImVec4(1.0f,0.3f,0.3f,1.0f),"Preview rebuild failed: %s",previewFailure.c_str());auto viewportSize=ImGui::GetContentRegionAvail();auto requestedWidth=(uint32_t)std::max(1.0f,viewportSize.x),requestedHeight=(uint32_t)std::max(1.0f,viewportSize.y);if(activePreviewTarget&&(requestedWidth!=viewportWidth||requestedHeight!=viewportHeight)){if(activePreviewTexture)provider->unregisterTexture(activePreviewTexture);activePreviewTarget->resize(requestedWidth,requestedHeight);viewportWidth=requestedWidth;viewportHeight=requestedHeight;scene->setViewport(0,0,viewportWidth,viewportHeight);camera->setAspectRatio(float(viewportWidth)/float(viewportHeight));if(auto texture=std::dynamic_pointer_cast<RenderTexture>(activePreviewTarget))activePreviewTexture=provider->registerTexture(texture);}if(activePreviewTexture)ImGui::Image(activePreviewTexture,ImVec2((float)viewportWidth,(float)viewportHeight),ImVec2(0,1),ImVec2(1,0));else ImGui::TextUnformatted("No presentation import is available."); ImGui::End();
			ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 24)); ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x,24)); ImGui::Begin("##Status",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings); ImGui::Text("%s%s | %.1f FPS | %d submitted triangles | %llu known unique | Preview: %s",openDocument?"Loaded":"Ready",(pipelineDirty||sceneDirty)?" *":"",fps,renderSystem.getCurrentRenderInfo().trianglesRendered,(unsigned long long)(openScene?openScene->getKnownTriangleCount():0),openDocument?(previewStale?"stale last-valid generation":"current generation"):"no document");if(openScene&&openScene->getUnknownTriangleModelCount()&&ImGui::IsItemHovered())ImGui::SetTooltip("%zu visible .mppmodel source(s) are not included until model metadata is loaded.",openScene->getUnknownTriangleModelCount()); ImGui::End();
			ImGui::Render(); provider->setDrawData(ImGui::GetDrawData());
			renderSystem.startStatsCollection(); renderSystem.renderScene(scene, camera, glm::vec2(0), activePipeline); renderer.render(&renderSystem); renderSystem.finishStatsCollection(); window.show();
		}
		if(activePreviewTexture)provider->unregisterTexture(activePreviewTexture);activePreviewTarget.reset();if(!activeGraphResource.empty()){renderSystem.removeRenderPipeline(activePipeline);resources.deleteResource(activeGraphResource);}
		scene->unload();scene.reset();sceneRuntime.clear(); imGuiShutdown(&backend); renderSystem.destroyCoreResources(); window.destroy(); SDL_Quit();
		return 0;
	}
	catch (std::exception const& error) { MessageBoxA(nullptr, error.what(), "PipelineEditor Error", MB_ICONERROR); return 1; }
}
