#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "utils/StructuredData.h"

#include "Config.h"

namespace mpp::resource_parsers
{
	// The result of converting the first material in a glTF document.
	struct _MPPRESOURCEPARSERSAPI GltfPbrMaterialLoadResult
	{
		utils::StructuredData materialDefinition{ "PbrMaterial" };
		std::string materialName;
		std::size_t materialIndex{ 0 };
		std::vector<std::string> warnings;
		std::vector<std::string> generatedImagePaths;
	};

	class _MPPRESOURCEPARSERSAPI GltfPbrMaterialLoader
	{
	public:
		static GltfPbrMaterialLoadResult loadFirstMaterial(std::string const& filepath);
	};
}
