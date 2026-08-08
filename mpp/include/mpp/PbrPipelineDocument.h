#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "utils/StructuredData.h"
#include "mpp/Config.h"
#include "mpp/Diagnostic.h"
#include "mpp/RenderGraph.h"

namespace mpp
{
	class RenderGraphPassFactoryRegistry;
	struct Caps;

	struct _MPPAPI PbrPreviewBinding
	{
		std::string binding;
		std::string materialResource;
	};

	struct _MPPAPI PbrPreviewOverride
	{
		std::string modelId;
		std::string binding;
		UniformCollection values;
	};

	enum class PbrPipelineResourceKind { PbrMaterial, Program, Texture, Sampler };

	struct _MPPAPI PbrPipelineResourceDocument
	{
		std::string name;
		PbrPipelineResourceKind kind{ PbrPipelineResourceKind::PbrMaterial };
		// Existing concrete resource XML payload. Its root name matches kind.
		utils::StructuredData definition{ "PbrMaterial" };
	};

	struct _MPPAPI PbrPipelineExternalResourceDocument
	{
		std::string libraryName;
		std::string libraryPath;
		PbrPipelineResourceDocument resource;
		bool readOnly{ true };
	};

	struct _MPPAPI PbrPipelineImportDocument
	{
		std::string id;
		std::string semantic;
		GraphImageFormat format{ GraphImageFormat::Rgba8 };
		GraphImageUsage usage{ GraphImageUsage::None };
		bool required{ true };
		std::string fallback;
	};

	struct _MPPAPI PbrPipelineExtensionDocument
	{
		std::string nameSpace;
		utils::StructuredData payload{ "Payload" };
	};

	struct _MPPAPI PbrPipelineEnvironmentDocument
	{
		std::string binding;
		std::string irradiance;
		std::string prefilteredSpecular;
		std::string brdfLut;
		std::string background;
	};

	struct _MPPAPI PbrPipelineBloomDocument
	{
		bool enabled{ false };
		uint32_t blurPasses{ 0 };
	};

	class _MPPAPI PbrPipelineDocument
	{
	public:
		static constexpr uint32_t CurrentVersion = 1;

		uint32_t version{ CurrentVersion };
		std::string name;
		std::string sourcePath;
		// Editor migration state; native serialization never emits this flag.
		bool importedFromRenderGraph{ false };
		std::string previewScene;
		std::vector<std::string> resourceLibraries;
		std::vector<PbrPipelineResourceDocument> localResources;
		// Resolved parser/runtime state; serialization emits only resourceLibraries.
		std::vector<PbrPipelineExternalResourceDocument> externalResources;
		std::vector<PbrPipelineImportDocument> imports;
		std::vector<PbrPipelineExtensionDocument> extensions;
		std::shared_ptr<RenderGraph> graph;
		PbrPipelineEnvironmentDocument environment;
		PbrPipelineBloomDocument bloom;
		std::vector<PbrPreviewBinding> previewBindings;
		std::vector<PbrPreviewOverride> previewOverrides;

		// Clones a read-only qualified library resource into the document and rewrites direct pipeline references.
		bool makeLocalCopy(std::string const& qualifiedName, std::string const& localName);
		DiagnosticBag validate(RenderGraphPassFactoryRegistry const* registry = nullptr) const;
		DiagnosticBag validate(Caps const& caps,RenderGraphPassFactoryRegistry const* registry = nullptr) const;
	};
}
