#include "mpp/RenderSystem.h"
#include "mpp/ResourceManager.h"
#include "mpp/Resource.h"
#include "mpp/StaticLogger.h"

using namespace std;

namespace mpp
{
	/*
	 * Constructor.
	 *
	 */
	Resource::Resource(string const& name, string const& type, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream)
		: mName(name)
		, mType(type)
		, mRefCount(0)
		, mCreated(false)
		, mLoaded(false)
		, mId(0)
		, mwRenderSystem(renderSystem)
		, mwResourceMgr(resourceMgr)
		, mResourceStream(resourceStream)
	{
		//mResourceStream->mwResource = this;
		//static_log_message(MPP_RESOURCE_LOGFILE, "Construct " + getType() + ": '" + getName() + "'");
	}

	Resource::~Resource()
	{
		//static_log_message(MPP_RESOURCE_LOGFILE, "Destruct " + getType() + ": '" + getName() + "'");
		destroy();
	}

	/*
	 * Set resource id.
	 *
	 */
	void Resource::setId(uint32_t id)
	{
		mId = id;
	}
	
	/*
	* Get the resource id.
	*
	*/
	uint32_t Resource::getId() const
	{
		return mId;
	}

	/*
	 * Get resource name.
	 *
	 */
	string const& Resource::getName() const
	{
		return mName;
	}

	/*
	 * Get resource type.
	 *
	 */
	string const& Resource::getType() const
	{
		return mType;
	}

	/*
	 * How many GL names does this resource manage?
	 *
	 */
	int Resource::getIdCount() const
	{
		return 0;
	}

	/*
	 * How many GL names are created?
	 *
	 */
	int Resource::getLiveIdCount() const
	{
		return 0;
	}

	/*
	 * Has the resource been created?
	 *
	 */
	bool Resource::isCreated() const
	{
		return mCreated;
	}

	/*
	 * Has the resource been loaded?
	 *
	 */
	bool Resource::isLoaded() const
	{
		return mLoaded;
	}

	/*
	 * Get the associated RenderSystem.
	 *
	 */
	RenderSystem* Resource::getRenderSystem()
	{
		return mwRenderSystem;
	}

	/*
	 * Get Resource Manager.
	 *
	 */
	ResourceManager* Resource::getResourceManager()
	{
		return mwResourceMgr;
	}

	/*
	 * Get the resource stream used to create this resource.
	 *
	 */
	ResourceStreamPtr Resource::getResourceStream()
	{
		return mResourceStream;
	}

	/*
	 * Add a tag to the resource.
	 *
	 */
	void Resource::addTag(string const& tag)
	{
		mTags.insert(tag);
	}

	/*
	 * Remove a tag from the resource.
	 *
	 */
	void Resource::removeTag(string const& tag)
	{
		mTags.erase(tag);
	}

	/*
	 * Does the resource have a particular tag?
	 *
	 */
	bool Resource::hasTag(string const& tag)
	{
		return mTags.find(tag) != mTags.end();
	}

	/*
	 * Create the resource.
	 *
	 */
	void Resource::create()
	{
		// Create child resources
		if (mResourceStream)
		{
			mResourceStream->createChildResources(getName());
		}

		if (!isCreated())
		{
			//static_log_message(MPP_RESOURCE_LOGFILE, "Create " + getType () + ": '" + getName() + "'");
			createImpl();
			mCreated = true;
		}
	}

	/*
	 * Destroy the resource.
	 *
	 */
	void Resource::destroy()
	{
		if (isLoaded())
		{
			unload();
		}

		if (isCreated())
		{
			//static_log_message(MPP_RESOURCE_LOGFILE, "Destroy " + getType() + ": '" + getName() + "'");
			destroyImpl();
			mCreated = false;
		}

		// Destroy child resources
		if (mResourceStream)
		{
			mResourceStream->destroyChildResources(getName());
		}
	}

	/*
 	 * Load the resource.
 	 *
 	 */
	void Resource::load(bool unloadStreamsAfterwards)
	{
		if (!isCreated())
		{
			create();
		}

		// Load child resources
		if (mResourceStream)
		{
			mResourceStream->loadChildResources(getName());
		}

		if (!isLoaded())
		{
			//static_log_message(MPP_RESOURCE_LOGFILE, "Load " + getType() + ": '" + getName() + "'");
			loadImpl();
			mLoaded = true;

			if (mResourceStream && unloadStreamsAfterwards)
			{
				mResourceStream->unload();
			}
		}
	}

	/*
	 * Unload the resource. 
	 *
	 */
	void Resource::unload()
	{
		if (isLoaded())
		{
			//static_log_message(MPP_RESOURCE_LOGFILE, "Unload " + getType() + ": '" + getName() + "'");
			unloadImpl();
			mLoaded = false;
		}

		// Unload child resources
		if (mResourceStream)
		{
			mResourceStream->unloadChildResources(getName());
		}
	}

	void Resource::acquire()
	{
		mRefCount++;
	}

	void Resource::release(bool destroyAfterUnload)
	{
		mRefCount--;

		if (mRefCount == 0)
		{
			unload();

			if (destroyAfterUnload)
			{
				destroy();
			}
		}
	}

}