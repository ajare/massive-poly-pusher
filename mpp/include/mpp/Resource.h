#pragma once

#include <string>
#include <set>
#include <memory>

#include "mpp/Config.h"
#include "mpp/ResourceStream.h"

namespace mpp
{
	class RenderSystem;
	class ResourceManager;
	
	class _MPPAPI Resource
	{
		std::string mName;

		std::string mType;

		std::set<std::string> mTags;

		bool mCreated;

		bool mLoaded;

		uint32 mId;

		RenderSystem* mwRenderSystem;

		ResourceManager* mwResourceMgr;

		ResourceStreamPtr mResourceStream;

	protected:

		virtual void createImpl() = 0;

		virtual void destroyImpl() = 0;

		virtual void loadImpl() = 0;

		virtual void unloadImpl() = 0;

		void setId(uint32 id);

	public:

		Resource(std::string const& name, std::string const& type, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		virtual ~Resource() = default;

		std::string const& getName() const;

		virtual std::string const& getType() const;

		bool isCreated() const;

		bool isLoaded() const;

		uint32 getId() const;

		RenderSystem* getRenderSystem();

		ResourceManager* getResourceManager();

		ResourceStreamPtr getResourceStream();

		void addTag(std::string const& tag);

		void removeTag(std::string const& tag);

		bool hasTag(std::string const& tag);

		void create();

		void destroy();

		void recreate();

		void load();

		void unload();
	};

	typedef std::shared_ptr<Resource> ResourcePtr;
}