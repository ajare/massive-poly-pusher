#pragma once

#include "Config.h"

namespace mpp
{
	class _MPPAPI ParticleSystem
	{
		int mMaxParticleCount;

		uint32 mCurVertexBuffer, mCurTransformFeedbackBuffer;

		uint32 mVertexBuffer[2];

		uint32 mTransformFeedback[2];

	public:

		explicit ParticleSystem(int maxParticleCount);

		void create();
	};
}
