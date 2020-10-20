#include "mpp/RenderSystem.h"
#include "mpp/Resource.h"

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
		, mCreated(false)
		, mLoaded(false)
		, mId(0)
		, mwRenderSystem(renderSystem)
		, mwResourceMgr(resourceMgr)
		, mResourceStream(resourceStream)
	{
	}

	/*
	 * Set resource id.
	 *
	 */
	void Resource::setId(uint32 id)
	{
		mId = id;
	}
	
	/*
	* Get the resource id.
	*
	*/
	uint32 Resource::getId() const
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
		mResourceStream->createChildResources(getName());

		if (!isCreated())
		{
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
			destroyImpl();
			mCreated = false;
		}

		// Destroy child resources
		mResourceStream->destroyChildResources(getName());
	}

	/*
	 * Recreate the resource.
	 *
	 */
	void Resource::recreate()
	{
		if (isLoaded())
		{
			unload();
		}

		if (isCreated())
		{
			destroy();
		}

		createImpl();
		mCreated = true;
	}

	/*
 	 * Load the resource.
 	 *
 	 */
	void Resource::load()
	{
		if (!isCreated())
		{
			create();
		}

		// Load child resources
		mResourceStream->loadChildResources(getName());

		if (!isLoaded())
		{
			loadImpl();
			mLoaded = true;
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
			unloadImpl();
			mLoaded = false;
		}

		// Unload child resources
		mResourceStream->unloadChildResources(getName());
	}
}