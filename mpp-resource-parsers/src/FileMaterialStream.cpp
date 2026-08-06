#include "utils/XmlReader.h"

#include "mpp/ResourceManager.h"
#include "mpp/resource-parsers/FileBasicMaterialStream.h"
#include "mpp/resource-parsers/FileMaterialStream.h"
#include "mpp/resource-parsers/FilePbrMaterialStream.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"

namespace mpp::resource_parsers
{
	ResourceStreamPtr FileMaterialStream::fromFile(ResourceManager* resourceMgr, std::string const& filepath, bool relativisePaths)
	{
		auto reader = utils::XmlReader::fromFile(filepath);
		auto data = reader->readTree();
		delete reader;
		auto const& root = data.getName();
		if (root == "BasicMaterial") return std::make_shared<FileBasicMaterialStream>(resourceMgr, filepath, relativisePaths);
		if (root == "PbrMaterial") return std::make_shared<FilePbrMaterialStream>(resourceMgr, filepath, relativisePaths);
		if (root == "Material")
		{
			if (resourceMgr) resourceMgr->warnMessage("Loading deprecated <Material> XML; rename and re-export it as BasicMaterial or PbrMaterial.");
			if (data.hasEntry("Pbr")) return std::make_shared<FilePbrMaterialStream>(resourceMgr, filepath, relativisePaths);
			return std::make_shared<FileBasicMaterialStream>(resourceMgr, filepath, relativisePaths);
		}
		THROW_MPP_RESOURCE_PARSERS("Material XML root must be BasicMaterial or PbrMaterial: " + filepath, __LINE__, __FILE__, __func__);
	}
}
