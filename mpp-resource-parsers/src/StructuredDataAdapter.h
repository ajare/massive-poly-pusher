#pragma once

#include "mpp/data/StructuredData.h"
#include "utils/StructuredData.h"

namespace mpp::resource_parsers::detail
{
	inline mpp::data::StructuredData importStructuredData(utils::StructuredData const& source)
	{
		if (source.isValue())
			return mpp::data::StructuredData(source.getName(), source.getValue());

		mpp::data::StructuredData result(source.getName());
		for (auto const& entry : source)
			result.addEntry(entry.first, importStructuredData(entry.second));
		return result;
	}
}
