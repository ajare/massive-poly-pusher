#pragma once

#include <vector>

#include "mpp/ResourceStream.h"

namespace mpp
{
	class _MPPAPI StringStream : public ResourceStream
	{
		struct QualitySetting
		{
		};

	protected:

		std::vector<QualitySetting> mQualitySettings;

	public:

		explicit StringStream(ResourceManager* resourceMgr);

		virtual std::string getData() const = 0;

		uint32_t createQualitySetting(std::string const& name);
	};
}