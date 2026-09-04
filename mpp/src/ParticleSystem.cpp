#include <algorithm>
#include <exception>
#include <string>

#include <GL/glew.h>
#include <GL/gl.h>

#include "mpp/ComputeProgram.h"
#include "mpp/GLErrorCheck.h"
#include "mpp/GpuDebugScope.h"
#include "mpp/MppException.h"
#include "mpp/ParticleDrawProgram.h"
#include "mpp/ParticleShaders.h"
#include "mpp/ParticleSystem.h"
#include "mpp/RawShaderStream.h"
#include "mpp/RenderSystem.h"
#include "mpp/ResourceManager.h"
#include "mpp/ShaderStorageBuffer.h"

using namespace std;

namespace mpp
{
	namespace
	{
		char const* SimulationProgramName = "__mpp_particle_simulation__";
		char const* DrawProgramName = "__mpp_particle_draw_additive__";

		// std430 binding points shared by the simulation kernel and the draw.
		constexpr uint32_t ParticlePoolBinding = 0;
		constexpr uint32_t IndirectCommandBinding = 1;

		// vec4 position and half-extent, for now.
		constexpr size_t ParticleStride = 16;
		// count, instanceCount, first, baseInstance.
		constexpr size_t IndirectCommandBytes = 4 * sizeof(uint32_t);
	}

	ParticleSystem::ParticleSystem(RenderSystem* renderSystem, ResourceManager* resourceManager)
		: mwRenderSystem(renderSystem)
		, mwResourceManager(resourceManager)
	{
	}

	ParticleSystem::~ParticleSystem()
	{
		if (mVertexArray != 0)
		{
			glDeleteVertexArrays(1, &mVertexArray);
			mVertexArray = 0;
		}

		mParticlePool.reset();
		mIndirectCommands.reset();

		// Mirrors RenderSystem::destroyCoreResources: release, then destroy what
		// nothing else still holds.
		auto releaseProgram = [this](ResourcePtr& program)
		{
			if (!program) return;
			program->release(mwRenderSystem);
			if (!program->isReferenced()) program->destroy();
			program.reset();
		};
		releaseProgram(mSimulationProgram);
		releaseProgram(mDrawProgram);
	}

	void ParticleSystem::initialise()
	{
		if (mInitialised) return;
		mInitialised = true;

		auto const& caps = mwRenderSystem->getCaps();
		if (!caps.supportsCompute)
		{
			mwRenderSystem->warnMessage("This OpenGL context reports no compute shader support. The particle system is disabled and no particles will be drawn.");
			return;
		}

		try
		{
			// Queried, not assumed: 64 is the size the kernel is written for, but a
			// context that permits fewer invocations gets a smaller group rather than
			// a link failure. The value reaches the kernel as a #define.
			mWorkGroupSize = max(1u, min<uint32_t>(64, min(caps.maxComputeWorkGroupSize[0], caps.maxComputeWorkGroupInvocations)));

			size_t const poolBytes = size_t(ParticleCount) * ParticleStride;
			if (caps.maxShaderStorageBufferBindings < 2)
			{
				THROW_MPP("The particle system needs two shader storage buffer bindings; this context reports " +
					to_string(caps.maxShaderStorageBufferBindings) + ".", __LINE__, __FILE__, __func__);
			}
			if (poolBytes > caps.maxShaderStorageBlockSize)
			{
				THROW_MPP("The particle pool needs " + to_string(poolBytes) + " bytes, exceeding the maximum shader storage block size of " +
					to_string(caps.maxShaderStorageBlockSize) + " bytes.", __LINE__, __FILE__, __func__);
			}

			mParticlePool = make_unique<ShaderStorageBuffer>();
			mParticlePool->create(poolBytes, nullptr, "Particle pool");
			mIndirectCommands = make_unique<ShaderStorageBuffer>();
			mIndirectCommands->create(IndirectCommandBytes, nullptr, "Particle indirect draw commands");

			auto simulationStream = make_shared<ComputeProgramStream>(mwResourceManager);
			simulationStream->setSource(RawShaderStage::Compute, ParticleSimulationComputeShader);
			simulationStream->setDefine("MPP_PARTICLE_WORK_GROUP_SIZE", to_string(mWorkGroupSize));
			mSimulationProgram = mwResourceManager->declareResource(SimulationProgramName, simulationStream).first;
			mSimulationProgram->acquire(mwRenderSystem);
			mSimulationProgram->load();

			auto drawStream = make_shared<ParticleDrawProgramStream>(mwResourceManager);
			drawStream->setSource(RawShaderStage::Vertex, ParticleDrawVertexShader);
			drawStream->setSource(RawShaderStage::Fragment, ParticleDrawFragmentShader);
			drawStream->setDefine("MPP_PARTICLE_BLEND_ADDITIVE", "1");
			mDrawProgram = mwResourceManager->declareResource(DrawProgramName, drawStream).first;
			mDrawProgram->acquire(mwRenderSystem);
			mDrawProgram->load();

			// A core-profile draw needs a bound vertex array object even when it
			// reads no attributes at all.
			GL_CHECK(glGenVertexArrays(1, &mVertexArray));
			if (mVertexArray == 0)
			{
				THROW_MPP("Could not create the particle vertex array object.", __LINE__, __FILE__, __func__);
			}

			mAvailable = true;
		}
		catch (exception const& error)
		{
			// A driver may report compute support and still refuse a valid kernel.
			// That is a degraded frame, not a failed one.
			mwRenderSystem->warnMessage(string("The particle system could not be initialised and is disabled; no particles will be drawn. ") + error.what());
			mAvailable = false;
		}
	}

	void ParticleSystem::simulate()
	{
		initialise();
		if (!mAvailable) return;

		GpuDebugScope scope("Particles: Simulate");

		auto* program = static_cast<ComputeProgram*>(mSimulationProgram.get());
		program->use();
		program->setUniform("PARTICLE_COUNT", ParticleCount);
		program->setUniform("ELAPSED_SECONDS", mwRenderSystem->getElapsedSeconds());

		mParticlePool->bindStorage(ParticlePoolBinding);
		mIndirectCommands->bindStorage(IndirectCommandBinding);

		program->dispatch((ParticleCount + mWorkGroupSize - 1) / mWorkGroupSize);

		// The pool is read by the vertex shader and the command buffer is read by
		// the indirect draw, so both consumers need naming here.
		GL_CHECK(glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT));
	}

	void ParticleSystem::render()
	{
		initialise();
		if (!mAvailable) return;

		GpuDebugScope scope("Particles: Draw");

		static_cast<ParticleDrawProgram*>(mDrawProgram.get())->use();

		mParticlePool->bindStorage(ParticlePoolBinding);
		mIndirectCommands->bindDrawIndirect();

		GL_CHECK(glBindVertexArray(mVertexArray));
		GL_CHECK(glDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr));
		GL_CHECK(glBindVertexArray(0));
		GL_CHECK(glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0));
	}
}
