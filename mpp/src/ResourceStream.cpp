#include "mpp/ResourceStream.h"

namespace mpp
{
	using namespace std;

	/*
	 * Constructor.
	 *
	 */
	ResourceStream::ResourceStream()
		: mLoaded(false)
	{
	}

	/*
	 * Destructor.
	 *
	 */
	ResourceStream::~ResourceStream()
	{
		unload();
	}

	/*
	 * Load resource stream data.
	 *
	 */
	void ResourceStream::load()
	{
		if (!mLoaded)
		{
			for (auto& child: mChildren)
			{
				child->load();
			}

			loadImpl();
			mLoaded = true;
		}
	}

	/*
	* Unload resource stream data.
	*
	*/
	void ResourceStream::unload()
	{
		if (mLoaded)
		{
			unloadImpl();
			mLoaded = false;

			for (auto& child: mChildren)
			{
				child->unload();
			}
		}
	}

	void ResourceStream::setFlags(uint32_t flags)
	{
		mFlags = flags;
	}

	uint32_t ResourceStream::getFlags() const
	{
		return mFlags;
	}

	void ResourceStream::addChild(ResourceStreamPtr child)
	{
		mChildren.push_back(child);
	}

	vector<ResourceStreamPtr> const& ResourceStream::getChildren() const
	{
		return mChildren;
	}
}