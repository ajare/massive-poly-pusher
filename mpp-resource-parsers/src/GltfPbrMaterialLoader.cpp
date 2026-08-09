#include "mpp/resource-parsers/GltfPbrMaterialLoader.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"

namespace mpp::resource_parsers
{
	GltfPbrMaterialLoadResult GltfPbrMaterialLoader::loadFirstMaterial(std::string const& filepath)
	{
		(void)filepath;
		THROW_MPP_RESOURCE_PARSERS_NOTIMP("glTF PBR material loading", __LINE__, __FILE__, __func__);
	}
}
