#pragma once

#include <vector>
#include <string>

#include "mpp/ResourceStream.h"
#include "mpp/UniformCollection.h"
#include "mpp/BasicMaterialSpecification.h"

namespace mpp
{
	class _MPPAPI BasicMaterialStream : public ResourceStream
	{
		friend class ResourceStreamSerializer;

	protected:
		struct QualitySetting { BasicMaterialSpecification spec; };
		std::string mName;
		std::vector<QualitySetting> mQualitySettings;

	public:
		explicit BasicMaterialStream(ResourceManager* resourceMgr);
		std::string const& getName() const;
		BasicMaterialSpecification::ProgramOptions const& getProgramOptions() const;
		mesh::MeshSpecification const& getMeshSpecification();
		UniformCollection const& getUniforms() const;
		std::vector<BasicMaterialSpecification::TextureOptions> const& getTextures() const;
		uint32_t createQualitySetting(std::string const& name) override;
	};
}
