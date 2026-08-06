#pragma once

#include <vector>
#include <map>
#include <set>

#include "mpp/ResourceStream.h"
#include "mpp/FileDataStream.h"
#include "mpp/UniformCollection.h"
#include "mpp/PbrMaterialSpecification.h"

#include "mpp/mesh/MeshSpecification.h"

namespace mpp
{
	class _MPPAPI PbrMaterialStream : public ResourceStream
	{
		friend class ResourceStreamSerializer;

	protected:

		struct QualitySetting
		{
			PbrMaterialSpecification spec;
		};

	protected:

		std::string mName;

		std::vector<QualitySetting> mQualitySettings;

	public:

		explicit PbrMaterialStream(ResourceManager* resourceMgr);

		std::string const& getName() const;

		PbrMaterialSpecification::ProgramOptions const& getProgramOptions() const;

		mesh::MeshSpecification const& getMeshSpecification();

		UniformCollection const& getUniforms() const;

		PbrMaterialSpecification::PbrSurface const& getPbrSurface() const;

		std::vector<PbrMaterialSpecification::TextureOptions> const& getTextures() const;

		uint32_t createQualitySetting(std::string const& name);
	};
}