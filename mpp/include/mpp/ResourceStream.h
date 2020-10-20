#pragma once

#include <memory>
#include <string>
#include <map>

#include "mpp/Config.h"

namespace mpp
{
	class ResourceManager;

	class _MPPAPI ResourceStream
	{
		bool mLoaded;

		uint32_t mFlags{ 0 };

		std::map<std::string, std::shared_ptr<ResourceStream>> mChildren;

		bool mChildrenCreated, mChildResourcesCreated, mChildResourcesLoaded;

		ResourceManager* mResourceMgr;

	private:

		virtual void loadImpl() = 0;

		// In case the user wants to free the data once
		// all resources have been created from it.
		virtual void unloadImpl() {}

		virtual void createChildResourceStreamsImpl() {};

		void createChildResourceStreams();

		void destroyChildResourceStreams();

	protected:

		ResourceManager* getResourceMgr();

	public:

		explicit ResourceStream(ResourceManager* resourceMgr);

		virtual ~ResourceStream();

		void addChild(std::string const& name, std::shared_ptr<ResourceStream> child);

		virtual std::string getType() = 0;

		void load();

		void unload();

		void setFlags(uint32_t flags);

		uint32_t getFlags() const;

		std::map<std::string, std::shared_ptr<ResourceStream>> const& getChildren() const;

		void createChildResources(std::string const& parentName);

		void destroyChildResources(std::string const& parentName);

		void loadChildResources(std::string const& parentName);

		void unloadChildResources(std::string const& parentName);
	};

	typedef std::shared_ptr<ResourceStream> ResourceStreamPtr;
}