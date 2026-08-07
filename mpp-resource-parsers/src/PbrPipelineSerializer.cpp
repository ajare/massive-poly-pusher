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
		writer.write(filepath);
	}
}
