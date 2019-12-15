#include "mpp/ResourceStream.h"

namespace mpp
{
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
		}
	}
}