#pragma once

#include <vector>
#include <map>
#include <set>

#include "mpp/ResourceStream.h"
#include "mpp/FileDataStream.h"
#include "mpp/UniformCollection.h"
#include "mpp/MaterialSpecification.h"

#include "mpp/mesh/MeshSpecification.h"

namespace mpp
{
	class _MPPAPI MaterialStream : public ResourceStream
	{
		friend class ResourceStreamSerializer;

	protected:

		struct QualitySetting
		{
			MaterialSpecification spec;
		};

	protected:

		std::string mName;
 
		std::vector<QualitySetting> mQualitySettings;

	public:

		explicit MaterialStream(ResourceManager* resourceMgr);

		std::string const& getName() const;

		MaterialSpecification::ProgramOptions const& getProgramOptions() const;

		UniformCollection const& getUniforms() const;

		std::map<std::string, std::pair<std::string, bool>> const& getTextures() const;

		uint32_t createQualitySetting(std::string const& name);
	};
}