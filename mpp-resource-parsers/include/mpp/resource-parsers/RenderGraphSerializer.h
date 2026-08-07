#pragma once

#include <string>

#include "mpp/RenderGraph.h"
#include "mpp/resource-parsers/Config.h"

namespace utils { class XmlWriteNode; }

namespace mpp
{
	namespace resource_parsers
	{
		class _MPPRESOURCEPARSERSAPI RenderGraphSerializer
		{
		public:
			static void toNode(RenderGraph const& graph, utils::XmlWriteNode* root);
			static void toFile(RenderGraph const& graph, std::string const& filepath);
		};
	}
}
