#pragma once

#include <memory>
#include <string>

#include "mpp/Config.h"

namespace mpp
{
	class _MPPAPI ResourceStream
	{
		bool mLoaded;

	private:

		virtual void loadImpl() = 0;

		// In case the user wants to free the data once
		// all resources have been created from it.
		virtual void unloadImpl() {}

	public:

		ResourceStream();

		virtual ~ResourceStream();

		virtual std::string getType() = 0;

		void load();

		void unload();
	};

	typedef std::shared_ptr<ResourceStream> ResourceStreamPtr;
}