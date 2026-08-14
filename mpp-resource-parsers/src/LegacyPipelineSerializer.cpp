#include <cstring>
#include <filesystem>
#include <sstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
#include "utils/XmlWriter.h"
#include "mpp/MppException.h"
#include "mpp/resource-parsers/LegacyPipelineSerializer.h"
#include "mpp/resource-parsers/RenderGraphSerializer.h"

namespace mpp::resource_parsers
{
	namespace
	{
		std::string usage(GraphImageUsage value){std::string result;auto add=[&](GraphImageUsage flag,char const*name){if(hasGraphImageUsage(value,flag)){if(!result.empty())result+=",";result+=name;}};add(GraphImageUsage::Sampled,"sampled");add(GraphImageUsage::ColourAttachment,"colourAttachment");add(GraphImageUsage::DepthAttachment,"depthAttachment");add(GraphImageUsage::Presentation,"presentation");return result;}
		std::string samples(std::optional<AntiAliasingSamples> const& value){return value?antiAliasingSamplesName(*value):"inherit";}
		std::string boolean(std::optional<bool> const& value){return value?(*value?"true":"false"):"inherit";}
		void writeData(mpp::data::StructuredData const& data,utils::XmlWriteNode* parent){for(auto const& entry:data){auto child=parent->createChild(entry.first);if(entry.second.isValue())child->setValue(entry.second.getValue());else writeData(entry.second,child);}}
		void writeUniforms(UniformCollection const& values,utils::XmlWriteNode* parent)
		{
			for(auto const& entry:values.getUniformData()){auto const& value=entry.second;if(value.count!=1)continue;std::string type;if(value.type==program::GLSLType::Int)type="Int";else if(value.type==program::GLSLType::Bool)type="Bool";else if(value.type==program::GLSLType::Float&&value.numElements>=1&&value.numElements<=4)type=value.numElements==1?"Float":value.numElements==2?"Vec2":value.numElements==3?"Vec3":"Vec4";else continue;auto node=parent->createChild(type);node->createChild("name")->setValue(entry.first);std::ostringstream text;if(value.type==program::GLSLType::Int||value.type==program::GLSLType::Bool){int32_t current;memcpy(&current,value.data,sizeof(current));if(value.type==program::GLSLType::Bool)text<<(current?"true":"false");else text<<current;}else{auto current=reinterpret_cast<float const*>(value.data);for(size_t i=0;i<value.numElements;++i){if(i)text<<' ';text<<current[i];}}node->createChild("value")->setValue(text.str());}
		}
	}
	void LegacyPipelineSerializer::toFile(LegacyPipelineDocument const& document, std::string const& filepath)
	{
		if (!document.graph) THROW_MPP("Cannot serialize LegacyPipeline without a RenderGraph.", __LINE__, __FILE__, __func__);
		utils::XmlWriter writer("LegacyPipeline");
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
		if(!document.localResources.empty())
		{
			auto resources=root->createChild("LocalResources");for(auto const& value:document.localResources){auto node=resources->createChild(value.definition.getName());writeData(value.definition,node);}
		}
		if(!document.imports.empty())
		{
			auto imports=root->createChild("Imports"); for(auto const& value:document.imports){auto node=imports->createChild("Import");node->createChild("id")->setValue(value.id);node->createChild("semantic")->setValue(value.semantic);node->createChild("format")->setValue(std::string(graphImageFormatName(value.format)));node->createChild("usage")->setValue(usage(value.usage));node->createChild("required")->setValue(value.required);if(!value.fallback.empty())node->createChild("fallback")->setValue(value.fallback);}
		}
		if(!document.outputs.empty())
		{
			auto outputs=root->createChild("Outputs");for(auto const& value:document.outputs){auto output=outputs->createChild("Output");output->createChild("name")->setValue(value.name);output->createChild("image")->setValue(value.image);if(!value.taaDepth.empty())output->createChild("taaDepth")->setValue(value.taaDepth);auto aa=output->createChild("AntiAliasing");aa->createChild("msaa")->setValue(samples(value.antiAliasing.msaa));aa->createChild("ssaa")->setValue(samples(value.antiAliasing.ssaa));aa->createChild("taa")->setValue(boolean(value.antiAliasing.taa));aa->createChild("fxaa")->setValue(boolean(value.antiAliasing.fxaa));}
		}
		auto bloom=root->createChild("Bloom");bloom->createChild("enabled")->setValue(document.bloom.enabled);bloom->createChild("blurPasses")->setValue(document.bloom.blurPasses);
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
		if(!document.previewOverrides.empty())
		{
			auto overrides=root->createChild("PreviewOverrides");for(auto const& value:document.previewOverrides){auto node=overrides->createChild("Override");node->createChild("model")->setValue(value.modelId);node->createChild("binding")->setValue(value.binding);if(!value.values.getUniformData().empty())writeUniforms(value.values,node->createChild("Values"));}
		}
		if(!document.extensions.empty())
		{
			auto extensions=root->createChild("Extensions");for(auto const& value:document.extensions){auto node=extensions->createChild("Extension");node->createChild("namespace")->setValue(value.nameSpace);writeData(value.payload,node->createChild("Payload"));}
		}
		auto graph = root->createChild("RenderGraph");
		RenderGraphSerializer::toNode(*document.graph, graph);
		auto temporary = filepath + ".tmp";
		writer.write(temporary);
#ifdef _WIN32
		auto from = std::filesystem::path(temporary).wstring(), to = std::filesystem::path(filepath).wstring();
		if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { std::filesystem::remove(temporary); THROW_MPP("Could not replace LegacyPipeline XML atomically.", __LINE__, __FILE__, __func__); }
#else
		std::filesystem::rename(temporary, filepath);
#endif
	}
}
