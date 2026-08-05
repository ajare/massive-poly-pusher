#pragma once

#include <string>

#include "mpp/RenderGraph.h"
#include "mpp/resource-parsers/Config.h"

namespace mpp
{
	namespace resource_parsers
	{
		class _MPPRESOURCEPARSERSAPI RenderGraphSerializer
		{
		public:
			static void toFile(RenderGraph const& graph, std::string const& filepath);
		};
	}
}
