#include <filesystem>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
#include "utils/XmlWriter.h"
#include "mpp/MppException.h"
#include "mpp/resource-parsers/PbrPipelineSerializer.h"
#include "mpp/resource-parsers/RenderGraphSerializer.h"

namespace mpp::resource_parsers
{
	void PbrPipelineSerializer::toFile(PbrPipelineDocument const& document, std::string const& filepath)
	{
		if (!document.graph) THROW_MPP("Cannot serialize PbrPipeline without a RenderGraph.", __LINE__, __FILE__, __func__);
		utils::XmlWriter writer("PbrPipeline");
		auto root = writer.getRootNode();
		root->addAttribute("version", document.version);
		root->createChild("version")->setValue(document.version); // StructuredData compatibility.
		root->createChild("name")->setValue(document.name);
		if (!document.previewScene.empty()) root->createChild("PreviewScene")->createChild("file")->setValue(document.previewScene);
		if (!document.resourceLibraries.empty())
		{
			auto libraries = root->createChild("ResourceLibraries");
			for (auto const& path : document.resourceLibraries) libraries->createChild("Library")->createChild("file")->setValue(path);
		}
		auto environment = root->createChild("Environment");
		environment->createChild("binding")->setValue(document.environment.binding);
		environment->createChild("irradiance")->setValue(document.environment.irradiance);
		environment->createChild("prefilteredSpecular")->setValue(document.environment.prefilteredSpecular);
		environment->createChild("brdfLut")->setValue(document.environment.brdfLut);
		environment->createChild("background")->setValue(document.environment.background);
		if (!document.previewBindings.empty())
		{
			auto bindings = root->createChild("PreviewBindings");
			for (auto const& value : document.previewBindings)
			{
				auto binding = bindings->createChild("Material");
				binding->createChild("binding")->setValue(value.binding);
				binding->createChild("resource")->setValue(value.materialResource);
			}
		}
		auto graph = root->createChild("RenderGraph");
		RenderGraphSerializer::toNode(*document.graph, graph);
		auto temporary = filepath + ".tmp";
		writer.write(temporary);
#ifdef _WIN32
		auto from = std::filesystem::path(temporary).wstring(), to = std::filesystem::path(filepath).wstring();
		if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { std::filesystem::remove(temporary); THROW_MPP("Could not replace PbrPipeline XML atomically.", __LINE__, __FILE__, __func__); }
#else
		std::filesystem::rename(temporary, filepath);
#endif
	}
}
