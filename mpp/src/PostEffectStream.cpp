#include <cassert>

#include "mpp/PostEffectStream.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	PostEffectStream::PostEffectStream(ResourceManager* resourceMgr)
		: ResourceStream(resourceMgr, "PostEffect")
	{
	}

	void PostEffectStream::loadImpl()
	{
		// Test gamma effect
		mInputs.push_back(
		{
			ImageType::Colour
		});

		mOutput.type = ImageType::Colour;
		mOutput.format = "RGB";
	}
}