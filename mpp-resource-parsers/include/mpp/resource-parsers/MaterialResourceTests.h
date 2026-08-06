#pragma once

#include <string>

#include "mpp/resource-parsers/Config.h"

namespace mpp { class ResourceManager; }

namespace mpp::resource_parsers
{
	// XML dispatch, typed binary round-trip, quality and legacy migration tests.
	_MPPRESOURCEPARSERSAPI bool runMaterialResourceTests(ResourceManager* resourceMgr, std::string* failure = nullptr);
}
