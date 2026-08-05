#include "mpp/MppException.h"
#include "mpp/RenderGraphPassFactoryRegistry.h"

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
}
