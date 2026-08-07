#include <memory>
#include <set>
#include <sstream>

#include "utils/XmlReader.h"
#include "utils/StringUtils.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"
#include "mpp/resource-parsers/PbrPipelineParser.h"
#include "mpp/resource-parsers/RenderGraphParser.h"

using namespace std;

namespace mpp::resource_parsers
{
	namespace
	{
		GraphImageFormat importFormat(std::string value)
		{
			utils::StringUtils::toUpper(value);
			if(value=="R8")return GraphImageFormat::R8;if(value=="RG8")return GraphImageFormat::Rg8;if(value=="RGBA8")return GraphImageFormat::Rgba8;if(value=="SRGB8_ALPHA8")return GraphImageFormat::Srgb8Alpha8;
			if(value=="R16F")return GraphImageFormat::R16f;if(value=="RG16F")return GraphImageFormat::Rg16f;if(value=="RGBA16F")return GraphImageFormat::Rgba16f;if(value=="R32F")return GraphImageFormat::R32f;if(value=="RG32F")return GraphImageFormat::Rg32f;if(value=="RGBA32F")return GraphImageFormat::Rgba32f;
			if(value=="R11G11B10F")return GraphImageFormat::R11g11b10f;if(value=="RGB10_A2")return GraphImageFormat::Rgb10a2;if(value=="DEPTH16")return GraphImageFormat::Depth16;if(value=="DEPTH24")return GraphImageFormat::Depth24;if(value=="DEPTH32F")return GraphImageFormat::Depth32f;if(value=="DEPTH24_STENCIL8")return GraphImageFormat::Depth24Stencil8;if(value=="DEPTH32F_STENCIL8")return GraphImageFormat::Depth32fStencil8;
			THROW_MPP_RESOURCE_PARSERS("Unknown PbrPipeline import format '"+value+"'.",__LINE__,__FILE__,__func__);
		}
		GraphImageUsage importUsage(std::string value)
		{
			GraphImageUsage usage=GraphImageUsage::None;std::stringstream stream(value);std::string token;
			while(std::getline(stream,token,',')){utils::StringUtils::toUpper(token);if(token=="SAMPLED")usage=usage|GraphImageUsage::Sampled;else if(token=="COLOURATTACHMENT")usage=usage|GraphImageUsage::ColourAttachment;else if(token=="DEPTHATTACHMENT")usage=usage|GraphImageUsage::DepthAttachment;else if(token=="PRESENTATION")usage=usage|GraphImageUsage::Presentation;else THROW_MPP_RESOURCE_PARSERS("Unknown PbrPipeline import usage '"+token+"'.",__LINE__,__FILE__,__func__);}
			return usage;
		}
		bool boolean(std::string value){utils::StringUtils::toUpper(value);if(value=="TRUE"||value=="1"||value=="YES")return true;if(value=="FALSE"||value=="0"||value=="NO")return false;THROW_MPP_RESOURCE_PARSERS("Invalid PbrPipeline boolean '"+value+"'.",__LINE__,__FILE__,__func__);}
		void rejectUnknown(utils::StructuredData const&,std::set<std::string> const&,std::string const&);
		void parseUniforms(utils::StructuredData const& data,UniformCollection& uniforms)
		{
			for(auto const& entry:data){auto const& value=entry.second;rejectUnknown(value,{"name","value"},"PreviewOverrides/Override/Values");auto name=value.getEntry("name").getValue();auto text=value.getEntry("value").getValue();std::istringstream input(text);
				if(entry.first=="Float")uniforms.setUniform(name,utils::StringUtils::parseFloat(text));else if(entry.first=="Int")uniforms.setUniform(name,(int32_t)utils::StringUtils::parseInt(text));else if(entry.first=="Bool"){int32_t current=boolean(text)?1:0;uniforms.setUniform(name,program::GLSLType::Bool,1,1,reinterpret_cast<char const*>(&current));}else if(entry.first=="Vec2"){glm::vec2 current;if(!(input>>current.x>>current.y))THROW_MPP_RESOURCE_PARSERS("Invalid Vec2 preview override.",__LINE__,__FILE__,__func__);uniforms.setUniform(name,current);}else if(entry.first=="Vec3"){glm::vec3 current;if(!(input>>current.x>>current.y>>current.z))THROW_MPP_RESOURCE_PARSERS("Invalid Vec3 preview override.",__LINE__,__FILE__,__func__);uniforms.setUniform(name,current);}else if(entry.first=="Vec4"){glm::vec4 current;if(!(input>>current.x>>current.y>>current.z>>current.w))THROW_MPP_RESOURCE_PARSERS("Invalid Vec4 preview override.",__LINE__,__FILE__,__func__);uniforms.setUniform(name,current);}else THROW_MPP_RESOURCE_PARSERS("Unknown preview override value type '"+entry.first+"'.",__LINE__,__FILE__,__func__);}
		}
		void rejectUnknown(utils::StructuredData const& data,std::set<std::string> const& allowed,std::string const& context){for(auto const& entry:data)if(!allowed.contains(entry.first))THROW_MPP_RESOURCE_PARSERS("Unknown field '"+entry.first+"' in "+context+".",__LINE__,__FILE__,__func__);}
		PbrPipelineResourceKind resourceKind(std::string const& value){if(value=="PbrMaterial")return PbrPipelineResourceKind::PbrMaterial;if(value=="Program")return PbrPipelineResourceKind::Program;if(value=="Texture")return PbrPipelineResourceKind::Texture;if(value=="Sampler")return PbrPipelineResourceKind::Sampler;THROW_MPP_RESOURCE_PARSERS("Unknown local resource type '"+value+"'.",__LINE__,__FILE__,__func__);}
	}
	PbrPipelineDocument PbrPipelineParser::fromFile(string const& filepath)
	{
		unique_ptr<utils::XmlReader> reader(utils::XmlReader::fromFile(filepath));
		auto data = reader->readTree();
		if (data.getName() != "PbrPipeline") THROW_MPP_RESOURCE_PARSERS("Pipeline root must be PbrPipeline: " + filepath, __LINE__, __FILE__, __func__);
		rejectUnknown(data,{"version","name","PreviewScene","ResourceLibraries","LocalResources","Imports","Environment","PreviewBindings","PreviewOverrides","RenderGraph"},"PbrPipeline");
		PbrPipelineDocument document;
		document.sourcePath = filepath;
		document.version = data.hasEntry("version") ? utils::StringUtils::parseUInt(data.getEntry("version").getValue()) : 1;
		document.name = data.hasEntry("name") ? data.getEntry("name").getValue() : "";
		if (data.hasEntry("PreviewScene")) { auto const& value=data.getEntry("PreviewScene");rejectUnknown(value,{"file"},"PreviewScene");document.previewScene = value.getEntry("file").getValue(); }
		if (data.hasEntry("ResourceLibraries"))
			for (auto const& entry : data.getEntry("ResourceLibraries")) { if(entry.first!="Library")THROW_MPP_RESOURCE_PARSERS("Unknown field '"+entry.first+"' in ResourceLibraries.",__LINE__,__FILE__,__func__);rejectUnknown(entry.second,{"file"},"ResourceLibraries/Library");document.resourceLibraries.push_back(entry.second.getEntry("file").getValue()); }
		if(data.hasEntry("LocalResources"))for(auto const& entry:data.getEntry("LocalResources"))
		{
			PbrPipelineResourceDocument resource;resource.kind=resourceKind(entry.first);resource.definition=entry.second;if(!entry.second.hasEntry("name"))THROW_MPP_RESOURCE_PARSERS("Local resource '"+entry.first+"' requires a name.",__LINE__,__FILE__,__func__);resource.name=entry.second.getEntry("name").getValue();document.localResources.push_back(resource);
		}
		if (data.hasEntry("Imports")) for(auto const& entry:data.getEntry("Imports"))
		{
			if(entry.first!="Import")THROW_MPP_RESOURCE_PARSERS("Unknown field '"+entry.first+"' in Imports.",__LINE__,__FILE__,__func__);
			auto const& value=entry.second;rejectUnknown(value,{"id","semantic","format","usage","required","fallback"},"Imports/Import"); PbrPipelineImportDocument import; import.id=value.getEntry("id").getValue(); import.semantic=value.getEntry("semantic").getValue(); import.format=importFormat(value.getEntry("format").getValue()); import.usage=importUsage(value.getEntry("usage").getValue()); if(value.hasEntry("required"))import.required=boolean(value.getEntry("required").getValue()); if(value.hasEntry("fallback"))import.fallback=value.getEntry("fallback").getValue(); document.imports.push_back(import);
		}
		if (data.hasEntry("Environment"))
		{
			auto const& environment = data.getEntry("Environment");
			rejectUnknown(environment,{"binding","irradiance","prefilteredSpecular","brdfLut","background"},"Environment");
			auto read = [&](char const* key) { return environment.hasEntry(key) ? environment.getEntry(key).getValue() : string(); };
			document.environment.binding = read("binding"); document.environment.irradiance = read("irradiance");
			document.environment.prefilteredSpecular = read("prefilteredSpecular"); document.environment.brdfLut = read("brdfLut"); document.environment.background = read("background");
		}
		if (data.hasEntry("PreviewBindings"))
			for (auto const& entry : data.getEntry("PreviewBindings")) { if(entry.first!="Material")THROW_MPP_RESOURCE_PARSERS("Unknown field '"+entry.first+"' in PreviewBindings.",__LINE__,__FILE__,__func__);rejectUnknown(entry.second,{"binding","resource"},"PreviewBindings/Material");document.previewBindings.push_back({ entry.second.getEntry("binding").getValue(), entry.second.getEntry("resource").getValue() }); }
		if(data.hasEntry("PreviewOverrides"))for(auto const& entry:data.getEntry("PreviewOverrides"))
		{
			if(entry.first!="Override")THROW_MPP_RESOURCE_PARSERS("Unknown field '"+entry.first+"' in PreviewOverrides.",__LINE__,__FILE__,__func__);auto const& value=entry.second;rejectUnknown(value,{"model","binding","Values"},"PreviewOverrides/Override");PbrPreviewOverride result;result.modelId=value.getEntry("model").getValue();result.binding=value.getEntry("binding").getValue();if(value.hasEntry("Values"))parseUniforms(value.getEntry("Values"),result.values);document.previewOverrides.push_back(result);
		}
		if (!data.hasEntry("RenderGraph")) THROW_MPP_RESOURCE_PARSERS("PbrPipeline has no embedded RenderGraph: " + filepath, __LINE__, __FILE__, __func__);
		document.graph = make_shared<RenderGraph>(RenderGraphParser::fromData(data.getEntry("RenderGraph"), filepath));
		return document;
	}
}
