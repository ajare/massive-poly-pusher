#pragma once

#include <memory>
#include <string>
#include <map>

#include "mpp/Config.h"

#define MPP_RESOURCE_LOGFILE "mpp-resources.log"

namespace mpp
{
	class ResourceManager;
	class Resource;

	class _MPPAPI ResourceStream
	{
		friend class Resource;
		friend class ResourceStreamSerializer;

	private:

		std::string mType;
		
		bool mLoaded;

		uint32_t mFlags{ 0 };

		std::map<std::string, std::shared_ptr<ResourceStream>> mChildren;

		bool mChildrenCreated, mChildResourcesCreated, mChildResourcesLoaded;

		ResourceManager* mResourceMgr;

		Resource* mwResource;

	protected:

		uint32_t mQualitySetting;

		std::map<std::string, uint32_t> mQualityNames;

	private:

		virtual void createChildResourceStreamsImpl() {};

		void createChildResourceStreams();

		void destroyChildResourceStreams();

	protected:

		virtual void loadImpl() = 0;

		virtual void unloadImpl() {}

		ResourceManager* getResourceMgr();

	public:

		ResourceStream(ResourceManager* resourceMgr, std::string const& type);

		virtual ~ResourceStream();

		void addChild(std::string const& name, std::shared_ptr<ResourceStream> child);

		std::string const& getType() const;

		void load(uint32_t qualitySetting);

		void unload();

		void setFlags(uint32_t flags);

		uint32_t getFlags() const;

		std::map<std::string, std::shared_ptr<ResourceStream>> const& getChildren() const;

		void createChildResources(std::string const& parentName);

		void destroyChildResources(std::string const& parentName);

		void loadChildResources(std::string const& parentName);

		void unloadChildResources(std::string const& parentName);

		virtual uint32_t createQualitySetting(std::string const& name) = 0;
	};

	typedef std::shared_ptr<ResourceStream> ResourceStreamPtr;
}