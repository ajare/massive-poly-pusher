#include <set>

#include "mpp/PbrPipelineDocument.h"
#include "mpp/RenderGraphPassFactoryRegistry.h"

using namespace std;

namespace mpp
{
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
			if (library.empty() || !libraries.insert(library).second) diagnostics.error("MPP-PIPELINE-005", "Resource library paths must be non-empty and unique.", { sourcePath }, "resources");
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
		}
		if (previewScene.empty()) diagnostics.warning("MPP-PIPELINE-008", "No preview scene is assigned.", { sourcePath }, "previewScene");
		if (environment.binding.empty()) diagnostics.warning("MPP-PIPELINE-009", "No logical PBR environment binding is assigned.", { sourcePath }, "environment");
		return diagnostics;
	}
}
