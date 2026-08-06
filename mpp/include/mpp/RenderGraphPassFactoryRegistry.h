#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "mpp/Config.h"

namespace mpp
{
	class RenderGraphExecutionContext;
	class RenderGraphScenePass;
	using RenderGraphPassCallback = std::function<void(RenderGraphExecutionContext const&)>;

	// Maps XML-authored identifiers to application-provided executable code.
	// Factories are registered by the application; code itself is never XML.
	class _MPPAPI RenderGraphPassFactoryRegistry
	{
		std::map<std::string, RenderGraphPassCallback> mFactories;
		std::map<std::string, std::function<std::unique_ptr<RenderGraphScenePass>()>> mScenePassFactories;

	public:
		void registerFactory(std::string const& name, RenderGraphPassCallback callback);
		void unregisterFactory(std::string const& name);
		RenderGraphPassCallback findFactory(std::string const& name) const;

		void registerScenePassFactory(std::string const& name, std::function<std::unique_ptr<RenderGraphScenePass>()> factory);
		std::unique_ptr<RenderGraphScenePass> createScenePass(std::string const& name) const;
	};
}
