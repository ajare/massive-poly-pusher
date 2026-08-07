#include <algorithm>
#include <filesystem>
#include <set>

#include "mpp/Caps.h"
#include "mpp/PbrPipelineDocument.h"
#include "mpp/RenderGraphPassFactoryRegistry.h"

using namespace std;

namespace mpp
{
	bool PbrPipelineDocument::makeLocalCopy(string const& qualifiedName,string const& localName)
	{
		if(localName.empty())return false;for(auto const& resource:localResources)if(resource.name==localName)return false;auto found=std::find_if(externalResources.begin(),externalResources.end(),[&](auto const& value){return value.libraryName+"::"+value.resource.name==qualifiedName;});if(found==externalResources.end())return false;auto local=found->resource;local.name=localName;utils::StructuredData renamed(local.definition.getName());for(auto const& entry:local.definition){if(entry.first=="name")renamed.addEntry("name",localName);else renamed.addEntry(entry.first,entry.second);}local.definition=renamed;localResources.push_back(local);
		auto rewrite=[&](string& value){if(value==qualifiedName)value=localName;};auto rewriteData=[&](auto&& self,utils::StructuredData& data)->void{if(data.isValue()){auto value=data.getValue();rewrite(value);data.setValue(value);}else for(auto& entry:data)self(self,entry.second);};for(auto& resource:localResources)rewriteData(rewriteData,resource.definition);for(auto& binding:previewBindings)rewrite(binding.materialResource);rewrite(environment.irradiance);rewrite(environment.prefilteredSpecular);rewrite(environment.brdfLut);rewrite(environment.background);for(auto& import:imports)rewrite(import.fallback);if(graph)for(uint32_t id=0;id<graph->getPassCount();++id){auto info=graph->getPassInfo({id});if(info.programResource==qualifiedName)graph->setPassProgramResource({id},localName);}return true;
	}

	DiagnosticBag PbrPipelineDocument::validate(RenderGraphPassFactoryRegistry const* registry) const
	{
		DiagnosticBag diagnostics;
		if (version != CurrentVersion) diagnostics.error("MPP-PIPELINE-001", "Unsupported PbrPipeline document version.", { sourcePath });
		if (name.empty()) diagnostics.error("MPP-PIPELINE-002", "PbrPipeline name is required.", { sourcePath }, "pipeline");
		if (!graph) diagnostics.error("MPP-PIPELINE-003", "PbrPipeline render graph is required.", { sourcePath }, "graph");
		else
		{
			auto compiled = graph->compile();
			for (auto const& message : compiled.diagnostics) diagnostics.error("MPP-PIPELINE-004", message, { sourcePath }, "graph");
			if (registry) diagnostics.append(registry->validate(*graph));
		}
		set<string> libraries;
		for (auto const& library : resourceLibraries)
		{
			if (library.empty() || !libraries.insert(library).second) diagnostics.error("MPP-PIPELINE-005", "Resource library paths must be non-empty and unique.", { sourcePath }, "resources");
			else { auto path=std::filesystem::path(library);if(path.is_absolute())diagnostics.warning("MPP-PIPELINE-014","Absolute resource-library path is not portable.",{sourcePath},library);auto resolved=path.is_absolute()?path:std::filesystem::path(sourcePath).parent_path()/path;if(!std::filesystem::exists(resolved))diagnostics.error("MPP-PIPELINE-015","Resource library does not exist: "+resolved.string(),{sourcePath},library); }
		}
		set<string> localNames;
		for(auto const& resource:localResources)
		{
			if(resource.name.empty()||!localNames.insert(resource.name).second)diagnostics.error("MPP-PIPELINE-022","Local resource names must be non-empty and unique.",{sourcePath},resource.name);auto expected=resource.kind==PbrPipelineResourceKind::PbrMaterial?"PbrMaterial":resource.kind==PbrPipelineResourceKind::Program?"Program":resource.kind==PbrPipelineResourceKind::Texture?"Texture":"Sampler";if(resource.definition.getName()!=expected)diagnostics.error("MPP-PIPELINE-023","Local resource '"+resource.name+"' payload type does not match its declared kind.",{sourcePath},resource.name);
		}
		set<string> externalNames;
		for(auto const& external:externalResources){auto qualified=external.libraryName+"::"+external.resource.name;if(external.libraryName.empty()||external.resource.name.empty())diagnostics.error("MPP-PIPELINE-025","External resource library and resource names are required.",{external.libraryPath},qualified);else if(!externalNames.insert(qualified).second)diagnostics.error("MPP-PIPELINE-026","Duplicate qualified external resource '"+qualified+"'.",{external.libraryPath},qualified);if(!external.readOnly)diagnostics.error("MPP-PIPELINE-027","External library resources must be read-only.",{external.libraryPath},qualified);}
		auto resourceReferenceIsResolvable=[&](string const& name){return localNames.contains(name)||externalNames.contains(name)||name.find('/')!=string::npos;};
		set<string> importIds;
		for(auto const& import:imports)
		{
			if(import.id.empty()||import.semantic.empty()||!importIds.insert(import.id).second) diagnostics.error("MPP-PIPELINE-010","Import IDs must be non-empty and unique and require a semantic.",{sourcePath},import.id);
			if(!import.required&&import.fallback.empty()) diagnostics.error("MPP-PIPELINE-011","Optional import '"+import.id+"' requires an explicit fallback.",{sourcePath},import.id);
			bool matched=false;
			if(graph) for(auto handle:graph->getImportedImages()){auto info=graph->getImageInfo(handle);if(info.importName==import.id||info.importName==import.semantic){matched=true;if(info.desc.format!=import.format||((uint32_t)info.desc.usage&(uint32_t)import.usage)!=(uint32_t)import.usage)diagnostics.error("MPP-PIPELINE-012","Import '"+import.id+"' graph descriptor is incompatible with its typed contract.",{sourcePath},import.id);}}
			if(!matched) diagnostics.warning("MPP-PIPELINE-013","Typed import '"+import.id+"' is not referenced by the graph.",{sourcePath},import.id);
		}
		set<string> bindings;
		for (auto const& binding : previewBindings)
		{
			if (binding.binding.empty() || binding.materialResource.empty()) diagnostics.error("MPP-PIPELINE-006", "Preview material binding and resource are required.", { sourcePath }, binding.binding);
			else if (!bindings.insert(binding.binding).second) diagnostics.error("MPP-PIPELINE-007", "Duplicate preview material binding '" + binding.binding + "'.", { sourcePath }, binding.binding);
			else if(!resourceReferenceIsResolvable(binding.materialResource))diagnostics.error("MPP-PIPELINE-024","Preview binding resource '"+binding.materialResource+"' is neither document-local nor externally qualified.",{sourcePath},binding.binding);
		}
		set<string> extensionNamespaces;for(auto const& extension:extensions)if(extension.nameSpace.empty()||!extensionNamespaces.insert(extension.nameSpace).second)diagnostics.error("MPP-PIPELINE-028","Extension namespaces must be non-empty and unique.",{sourcePath},extension.nameSpace);
		set<string> overrideTargets;
		for(auto const& value:previewOverrides)
		{
			auto target=value.modelId+"\n"+value.binding;if(value.modelId.empty()||value.binding.empty())diagnostics.error("MPP-PIPELINE-018","Preview overrides require model and binding IDs.",{sourcePath},value.modelId);else if(!overrideTargets.insert(target).second)diagnostics.error("MPP-PIPELINE-019","Duplicate preview override for model '"+value.modelId+"' and binding '"+value.binding+"'.",{sourcePath},value.modelId);if(!value.binding.empty()&&!bindings.contains(value.binding))diagnostics.error("MPP-PIPELINE-020","Preview override references unknown binding '"+value.binding+"'.",{sourcePath},value.modelId);
			for(auto const& uniform:value.values.getUniformData()){auto const& data=uniform.second;bool supported=data.count==1&&((data.type==program::GLSLType::Int||data.type==program::GLSLType::Bool)||(data.type==program::GLSLType::Float&&data.numElements>=1&&data.numElements<=4));if(!supported)diagnostics.error("MPP-PIPELINE-021","Preview override value '"+uniform.first+"' has an unsupported type or array shape.",{sourcePath},value.modelId);}
		}
		if (previewScene.empty()) diagnostics.warning("MPP-PIPELINE-008", "No preview scene is assigned.", { sourcePath }, "previewScene");
		else {auto path=std::filesystem::path(previewScene);if(path.is_absolute())diagnostics.warning("MPP-PIPELINE-016","Absolute preview-scene path is not portable.",{sourcePath},"previewScene");auto resolved=path.is_absolute()?path:std::filesystem::path(sourcePath).parent_path()/path;if(!std::filesystem::exists(resolved))diagnostics.error("MPP-PIPELINE-017","Preview scene does not exist: "+resolved.string(),{sourcePath},"previewScene");}
		if (environment.binding.empty()) diagnostics.warning("MPP-PIPELINE-009", "No logical PBR environment binding is assigned.", { sourcePath }, "environment");
		return diagnostics;
	}

	DiagnosticBag PbrPipelineDocument::validate(Caps const& caps,RenderGraphPassFactoryRegistry const* registry) const
	{
		auto diagnostics=validate(registry);if(graph){auto compiled=graph->compile(caps);for(auto const& message:compiled.diagnostics)diagnostics.error("MPP-PIPELINE-029",message,{sourcePath},"graph");}return diagnostics;
	}
}
