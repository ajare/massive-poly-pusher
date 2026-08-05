#pragma once

#include <functional>
#include <map>
#include <string>

#include "mpp/Config.h"

namespace mpp
{
	class RenderGraphExecutionContext;
	using RenderGraphPassCallback = std::function<void(RenderGraphExecutionContext const&)>;

	// Maps XML-authored identifiers to application-provided executable code.
	// Factories are registered by the application; code itself is never XML.
	class _MPPAPI RenderGraphPassFactoryRegistry
	{
		std::map<std::string, RenderGraphPassCallback> mFactories;

	public:
		void registerFactory(std::string const& name, RenderGraphPassCallback callback);
		void unregisterFactory(std::string const& name);
		RenderGraphPassCallback findFactory(std::string const& name) const;
	};
}
