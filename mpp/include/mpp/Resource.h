#pragma once

#include <string>
#include <set>
#include <cstdint>
#include <memory>

#include "mpp/Config.h"
#include "mpp/ResourceStream.h"
#include "mpp/ResourceWrangler.h"

namespace mpp
{
	class RenderSystem;
	class ResourceManager;
	
	class _MPPAPI Resource : public ResourceWrangler
	{
		std::string mName;

		std::string mType;

		int mRefCount;

		std::set<std::string> mTags;

		bool mCreated;

		bool mLoaded;
		uint64_t mLifecycleRevision{ 1 };

		uint32_t mId;

		RenderSystem* mwRenderSystem;

		ResourceManager* mwResourceMgr;

		ResourceStreamPtr mResourceStream;

		std::set<std::shared_ptr<Resource>> mDependentResources;

		std::set<ResourceWrangler*> mDependingResources;

	protected:

		virtual void createImpl() = 0;

		virtual void destroyImpl() = 0;

		virtual void loadImpl() = 0;

		virtual void unloadImpl() = 0;

		void setId(uint32_t id);

		void acquireDependentResource(std::shared_ptr<Resource> resource);

		void releaseDependentResources();

	public:

		Resource(std::string const& name, std::string const& type, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		virtual ~Resource();

		std::string const& getName() const;

		virtual std::string const& getType() const;

		bool isCreated() const;

		bool isLoaded() const;

		uint64_t getLifecycleRevision() const;

		bool isReferenced() const;
		
		uint32_t getId() const;

		virtual int getIdCount() const;

		virtual int getLiveIdCount() const;

		int getRefCount() const;

		int getDependentResourceCount() const;

		int getDependingObjectCount() const;

		std::set<std::shared_ptr<Resource>> const& getDependentResources() const;

		std::set<ResourceWrangler*> const& getDependingResources() const;

		RenderSystem* getRenderSystem();

		ResourceManager* getResourceManager();

		ResourceStreamPtr getResourceStream();

		void addTag(std::string const& tag);

		void removeTag(std::string const& tag);

		bool hasTag(std::string const& tag);

		void create();

		void destroy();

		void load(bool unloadStreamsAfterwards = false);

		void unload();

		// Re-reads the resource from its source and rebuilds it to the state it
		// is already in, without disturbing anything that holds it.
		//
		// The wrangler count is deliberately not consulted. That count exists to
		// stop a resource being torn down while something still uses it, and a
		// refresh is not a teardown - it returns the resource to the state it
		// would have been in had it just been read, which is a thing a shared
		// resource can safely have done to it.
		//
		// The position is preserved rather than pushed forward: a resource that
		// was created and loaded comes back created and loaded, one that was only
		// created comes back only created, and one that was neither just gets a
		// fresh stream for whatever creates it later. Forcing a load would be
		// wrong twice over - it would move the resource to a state its holders
		// did not ask for, and it would try to upload to a render system that
		// may not exist.
		void refresh();

		void acquire(ResourceWrangler* acquirer);

		void release(ResourceWrangler* releaser);
	};

	typedef std::shared_ptr<Resource> ResourcePtr;
}