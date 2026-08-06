#include "mpp/MppException.h"
#include "mpp/RenderGraphPassFactoryRegistry.h"
#include "mpp/RenderGraphScenePass.h"

namespace mpp
{
	void RenderGraphPassFactoryRegistry::registerFactory(std::string const& name, RenderGraphPassCallback callback)
	{
		if (name.empty() || !callback)
		{
			THROW_MPP("Render graph callback factory requires a name and callback.", __LINE__, __FILE__, __func__);
		}
		mFactories[name] = std::move(callback);
	}

	void RenderGraphPassFactoryRegistry::unregisterFactory(std::string const& name)
	{
		mFactories.erase(name);
	}

	RenderGraphPassCallback RenderGraphPassFactoryRegistry::findFactory(std::string const& name) const
	{
		auto const found = mFactories.find(name);
		return found == mFactories.end() ? RenderGraphPassCallback() : found->second;
	}

	void RenderGraphPassFactoryRegistry::registerScenePassFactory(std::string const& name, std::function<std::unique_ptr<RenderGraphScenePass>()> factory)
	{
		if (name.empty() || !factory)
		{
			THROW_MPP("Render graph scene-pass factory requires a name and factory.", __LINE__, __FILE__, __func__);
		}
		mScenePassFactories[name] = std::move(factory);
	}

	std::unique_ptr<RenderGraphScenePass> RenderGraphPassFactoryRegistry::createScenePass(std::string const& name) const
	{
		auto const found = mScenePassFactories.find(name);
		return found == mScenePassFactories.end() ? nullptr : found->second();
	}
}
