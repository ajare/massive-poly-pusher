#include <stdexcept>

#include "mpp/resource-parsers/GltfPbrMaterialLoader.h"

namespace mpp::resource_parsers
{
	GltfPbrMaterialLoadResult GltfPbrMaterialLoader::loadFirstMaterial(std::filesystem::path const& filepath)
	{
		if (filepath.empty())
			throw std::invalid_argument("glTF material loader requires a file path.");
		throw std::runtime_error("glTF PBR material loading is not implemented yet (phase 1 API only): " + filepath.string());
	}
}
