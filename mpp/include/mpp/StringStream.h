#pragma once

#include <vector>

#include "mpp/ResourceStream.h"

namespace mpp
{
	class _MPPAPI StringStream : public ResourceStream
	{
		friend class ResourceStreamSerializer;

	protected:

		struct QualitySetting
		{
			std::string data, file;
			bool isFile;
		};

	protected:

		std::vector<QualitySetting> mQualitySettings;

	public:

		explicit StringStream(ResourceManager* resourceMgr);

		uint32_t createQualitySetting(std::string const& name);

		std::string const& getString() const;
	};
}