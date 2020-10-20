#include "mpp/ResourceStream.h"
#include "mpp/ResourceManager.h"

namespace mpp
{
	using namespace std;

	/*
	 * Constructor.
	 *
	 */
	ResourceStream::ResourceStream(ResourceManager* resourceMgr)
		: mLoaded(false)
		, mChildrenCreated(false)
		, mResourceMgr(resourceMgr)
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

	ResourceManager* ResourceStream::getResourceMgr()
	{
		return mResourceMgr;
	}

	void ResourceStream::createChildResourceStreams()
	{
		if (!mChildrenCreated)
		{
			createChildResourceStreamsImpl();
			mChildrenCreated = true;
		}
	}

	void ResourceStream::destroyChildResourceStreams()
	{
		if (mChildrenCreated)
		{
			mChildren.clear();
			mChildrenCreated = false;
		}
	}

	/*
	 * Load resource stream data.
	 *
	 */
	void ResourceStream::load()
	{
		createChildResourceStreams();

		if (!mLoaded)
		{
			for (auto& child: mChildren)
			{
				child.second->load();
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
				child.second->unload();
			}
		}

		destroyChildResourceStreams();
	}

	void ResourceStream::setFlags(uint32_t flags)
	{
		mFlags = flags;
	}

	uint32_t ResourceStream::getFlags() const
	{
		return mFlags;
	}

	void ResourceStream::addChild(string const& name, ResourceStreamPtr child)
	{
		mChildren[name] = child;
	}

	map<string, ResourceStreamPtr> const& ResourceStream::getChildren() const
	{
		return mChildren;
	}

	void ResourceStream::createChildResources(string const& parentName)
	{
		for (auto child: mChildren)
		{
			string name = parentName + "/" + child.first;
			auto res = child.second;

			res->createChildResources(name);
			mResourceMgr->createResource(name, res);
		}
	}

	void ResourceStream::destroyChildResources(string const& parentName)
	{
		for (auto child: mChildren)
		{
			string name = parentName + "/" + child.first;
			auto res = mResourceMgr->getResource(name);
			res->destroy();

			child.second->destroyChildResources(name);
		}
	}

	void ResourceStream::loadChildResources(string const& parentName)
	{
		for (auto child: mChildren)
		{
			string name = parentName + "/" + child.first;
			child.second->loadChildResources(name);

			auto res = mResourceMgr->getResource(name);
			res->load();
		}
	}

	void ResourceStream::unloadChildResources(string const& parentName)
	{
		for (auto child: mChildren)
		{
			string name = parentName + "/" + child.first;
			auto res = mResourceMgr->getResource(name);
			res->unload();

			child.second->unloadChildResources(name);
		}
	}}