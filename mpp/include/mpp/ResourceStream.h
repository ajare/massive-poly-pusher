#pragma once

#include <memory>
#include <string>
#include <map>

#include "mpp/Config.h"

namespace mpp
{
	class ResourceManager;
	class Resource;

	enum class ResourceStreamEvent
	{
		Load,
		Unload,
	};

	class _MPPAPI ResourceStream
	{
		friend class Resource;

	private:

		std::string mType;
		
		bool mLoaded;

		uint32_t mFlags{ 0 };

		std::map<std::string, std::shared_ptr<ResourceStream>> mChildren;

		bool mChildrenCreated, mChildResourcesCreated, mChildResourcesLoaded;

		ResourceManager* mResourceMgr;

		Resource* mwResource;

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

		ResourceStream(ResourceManager* resourceMgr, std::string const& type);

		virtual ~ResourceStream();

		void addChild(std::string const& name, std::shared_ptr<ResourceStream> child);

		std::string const& getType() const;

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