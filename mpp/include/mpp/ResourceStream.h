#pragma once

#include <memory>
#include <string>
#include <vector>

#include "mpp/Config.h"

namespace mpp
{
	class _MPPAPI ResourceStream
	{
		bool mLoaded;

		uint32_t mFlags{ 0 };

		std::vector<std::shared_ptr<ResourceStream>> mChildren;

	private:

		virtual void loadImpl() = 0;

		// In case the user wants to free the data once
		// all resources have been created from it.
		virtual void unloadImpl() {}

	protected:

		void addChild(std::shared_ptr<ResourceStream> child);

	public:

		ResourceStream();

		virtual ~ResourceStream();

		virtual std::string getType() = 0;

		void load();

		void unload();

		void setFlags(uint32_t flags);

		uint32_t getFlags() const;

		std::vector<std::shared_ptr<ResourceStream>> const& getChildren() const;
	};

	typedef std::shared_ptr<ResourceStream> ResourceStreamPtr;
}