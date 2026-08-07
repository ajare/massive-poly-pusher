#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "mpp/Config.h"
#include "mpp/Diagnostic.h"
#include "mpp/RenderGraph.h"

namespace mpp
{
	class RenderGraphPassFactoryRegistry;

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

	struct _MPPAPI PbrPipelineEnvironmentDocument
	{
		std::string binding;
		std::string irradiance;
		std::string prefilteredSpecular;
		std::string brdfLut;
		std::string background;
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
		std::shared_ptr<RenderGraph> graph;
		PbrPipelineEnvironmentDocument environment;
		std::vector<PbrPreviewBinding> previewBindings;
		std::vector<PbrPreviewOverride> previewOverrides;

		DiagnosticBag validate(RenderGraphPassFactoryRegistry const* registry = nullptr) const;
	};
}
