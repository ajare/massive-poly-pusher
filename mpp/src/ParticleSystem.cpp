#include "ParticleSystem.h"
#include "Particle.h"

using namespace std;

namespace mpp
{
	/*
	* Constructor.
	*
	*/
	ParticleSystem::ParticleSystem(int maxParticleCount)
		: mMaxParticleCount(maxParticleCount)
		, mCurVertexBuffer(0)
		, mCurTransformFeedbackBuffer(0)
	{
		mVertexBuffer[0] = mVertexBuffer[1] = 0;
		mTransformFeedback[0] = mTransformFeedback[1] = 0;
	}

	/*
	 * Create particle system in GPU memory.
	 *
	 */
	void ParticleSystem::create()
	{
		Particle* particles = new Particle[mMaxParticleCount];
	}
}