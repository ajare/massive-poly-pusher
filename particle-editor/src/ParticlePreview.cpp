#include "ParticlePreview.h"

#include <algorithm>
#include <stdexcept>

#include <glm/geometric.hpp>

#include <mpp/Camera.h>
#include <mpp/Colour.h>
#include <mpp/ParticleEffectValidator.h>
#include <mpp/ProgrammaticParticleEffectStream.h>
#include <mpp/ProgrammaticRenderTextureStream.h>
#include <mpp/RenderGraphStream.h>
#include <mpp/RenderPipeline.h>
#include <mpp/RenderSystem.h>
#include <mpp/RenderTexture.h>
#include <mpp/ResourceManager.h>
#include <mpp/Scene.h>
#include <mpp/resource-parsers/FileRenderGraphStream.h>

namespace particle_editor
{
	namespace
	{
		constexpr char PreviewPipeline[] = "ParticleEditor.PbrPreview";
		constexpr char PreviewGraph[] = "ParticleEditor.PbrGraph";
		constexpr char PreviewTarget[] = "ParticleEditor.Presentation";
	}

	ParticlePreview::ParticlePreview(mpp::RenderSystem* renderSystem, mpp::ResourceManager* resources)
		: mRenderSystem(renderSystem), mResources(resources)
	{
	}

	ParticlePreview::~ParticlePreview()
	{
		shutdown();
	}

	void ParticlePreview::initialise(std::filesystem::path const& resourceRoot, uint32_t width, uint32_t height)
	{
		if (mInitialised) return;
		if (!mRenderSystem || !mResources) throw std::invalid_argument("Particle preview requires MPP services.");
		mWidth = std::max(64u, width);
		mHeight = std::max(64u, height);

		auto presentationStream = std::make_shared<mpp::ProgrammaticRenderTextureStream>(mResources);
		presentationStream->setTarget(mpp::TextureTarget::Texture2D);
		presentationStream->setInternalFormat(mpp::TextureInternalType::UnsignedInteger, true, 8u, 4u);
		presentationStream->setWidth(mWidth);
		presentationStream->setHeight(mHeight);
		presentationStream->setNumAttachments(1u);
		presentationStream->setFiltering(mpp::TextureParams::MinFilter::Linear, mpp::TextureParams::MagFilter::Linear);
		mPresentationResource = mResources->declareResource(PreviewTarget, presentationStream).first;
		mPresentationResource->load();
		mPresentationTexture = std::dynamic_pointer_cast<mpp::RenderTexture>(mPresentationResource);
		if (!mPresentationTexture) throw std::runtime_error("Particle preview presentation target is not a render texture.");

		auto graphPath = resourceRoot / "demo-suite" / "res" / "ParticlePbr.rendergraph.yaml";
		if (!std::filesystem::is_regular_file(graphPath))
			throw std::runtime_error("Particle Editor PBR graph was not found: " + graphPath.string());
		auto graphStream = std::make_shared<mpp::resource_parsers::FileRenderGraphStream>(mResources, graphPath.string());
		mGraphResource = mResources->declareResource(PreviewGraph, graphStream).first;
		mGraphResource->load();

		mpp::RenderPipelineOptions options;
		options.mode = mpp::RenderPipelineMode::XmlGraphPbrForward;
		options.graphTemplate = mGraphResource;
		options.graphImports["screen"] = mPresentationTexture;
		mRenderSystem->getOrCreateRenderPipeline(PreviewPipeline, options);

		mScene = std::make_shared<mpp::Scene>(mRenderSystem);
		mScene->load();
		mScene->setClearColour(mpp::Colour(0.025f, 0.035f, 0.055f));
		mScene->setViewport(0, 0, mWidth, mHeight);
		mpp::PbrLight light;
		light.direction = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
		light.intensity = 2.5f;
		mScene->setPbrLights({ light });
		mCamera = std::make_shared<mpp::Camera>(glm::vec3(0.0f, 2.6f, 7.0f), 0.0f, 0.0f, 0.0f,
			55.0f, float(mWidth) / float(mHeight));
		mCamera->setLookAt({ 0.0f, 2.6f, 7.0f }, { 0.0f, 1.5f, 0.0f });
		mCamera->setClipDistances(0.1f, 100.0f);

		auto& particles = mRenderSystem->getParticleSystem();
		particles.initialise();
		particles.setStatisticsEnabled(true);
		mInitialised = true;
	}

	bool ParticlePreview::install(mpp::ParticleEffectSpecification const& specification, std::string* failure)
	{
		if (!mInitialised)
		{
			if (failure) *failure = "Particle preview has not been initialised.";
			return false;
		}
		auto diagnostics = mpp::ParticleEffectValidator::validate(specification);
		if (diagnostics.hasErrors())
		{
			if (failure)
			{
				*failure = "The particle effect is invalid.";
				for (auto const& diagnostic : diagnostics.getDiagnostics())
					if (diagnostic.severity == mpp::DiagnosticSeverity::Error)
					{
						*failure += " [" + diagnostic.code + "] " + diagnostic.message;
						break;
					}
			}
			return false;
		}

		auto candidateName = "ParticleEditor.Effect." + std::to_string(++mEffectGeneration);
		mpp::ResourcePtr candidate;
		mpp::ParticleEffectHandle candidateEffect;
		try
		{
			auto stream = std::make_shared<mpp::ProgrammaticParticleEffectStream>(mResources);
			stream->setSpecification(specification);
			candidate = mResources->declareResource(candidateName, stream).first;
			candidate->load();
			candidateEffect = mRenderSystem->getParticleSystem().createEffect(candidate);

			auto oldResource = mEffectResource;
			auto oldEffect = mEffect;
			mEffectResource = std::move(candidate);
			mEffect = candidateEffect;
			if (oldEffect && mRenderSystem->getParticleSystem().isAlive(oldEffect))
				mRenderSystem->getParticleSystem().destroyEffect(oldEffect);
			if (oldResource)
			{
				auto oldName = oldResource->getName();
				oldResource.reset();
				mResources->deleteResourceTree(oldName);
			}
			++mRebuildCount;
			if (failure) failure->clear();
			return true;
		}
		catch (std::exception const& error)
		{
			if (candidateEffect && mRenderSystem->getParticleSystem().isAlive(candidateEffect))
				mRenderSystem->getParticleSystem().destroyEffect(candidateEffect);
			if (candidate)
			{
				auto name = candidate->getName();
				candidate.reset();
				try { mResources->deleteResourceTree(name); } catch (...) {}
			}
			if (failure) *failure = error.what();
			return false;
		}
	}

	bool ParticlePreview::updateLive(mpp::ParticleEffectSpecification const& specification, std::string* failure)
	{
		if (!ready())
		{
			if (failure) *failure = "The live particle preview is not available.";
			return false;
		}
		auto diagnostics = mpp::ParticleEffectValidator::validate(specification);
		if (diagnostics.hasErrors())
		{
			if (failure) *failure = "The particle effect is invalid.";
			return false;
		}

		auto& particles = mRenderSystem->getParticleSystem();
		for (size_t index = 0; index < specification.emitterTemplates.size(); ++index)
			if (!particles.getEmitter(mEffect, index))
			{
				if (failure) *failure = "The live emitter-template structure no longer matches the document.";
				return false;
			}
		for (size_t index = 0; index < specification.emitterTemplates.size(); ++index)
		{
			auto const& emitter = specification.emitterTemplates[index].value;
			particles.updateEmitterTemplateRuntime(particles.getEmitter(mEffect, index),
				emitter.simulation, emitter.appearance);
		}
		++mLiveUpdateCount;
		if (failure) failure->clear();
		return true;
	}

	void ParticlePreview::resize(uint32_t width, uint32_t height)
	{
		if (!mInitialised || width < 64u || height < 64u || (width == mWidth && height == mHeight)) return;
		mPresentationTexture->resize(width, height);
		mRenderSystem->getRenderPipeline(PreviewPipeline)->resize(width, height);
		mScene->setViewport(0, 0, width, height);
		mCamera->setAspectRatio(float(width) / float(height));
		mCamera->markCut();
		mWidth = width;
		mHeight = height;
	}

	void ParticlePreview::pauseSimulation()
	{
		mRenderSystem->getParticleSystem().pauseSimulation();
	}

	void ParticlePreview::resumeSimulation()
	{
		mRenderSystem->getParticleSystem().resumeSimulation();
	}

	void ParticlePreview::stepSimulation(float deltaSeconds)
	{
		mRenderSystem->getParticleSystem().requestSimulationStep(deltaSeconds);
	}

	void ParticlePreview::setSimulationTimeScale(float scale)
	{
		mRenderSystem->getParticleSystem().setSimulationTimeScale(scale);
	}

	bool ParticlePreview::isSimulationPaused() const
	{
		return mRenderSystem->getParticleSystem().isSimulationPaused();
	}

	float ParticlePreview::simulationTimeScale() const
	{
		return mRenderSystem->getParticleSystem().getSimulationTimeScale();
	}

	void ParticlePreview::render()
	{
		if (ready()) mRenderSystem->renderScene(mScene, mCamera, glm::vec2(0.0f), PreviewPipeline);
	}

	mpp::ParticleStats const& ParticlePreview::stats() const
	{
		return mRenderSystem->getParticleSystem().getStats();
	}

	bool ParticlePreview::ready() const
	{
		return mInitialised && bool(mEffectResource) &&
			mRenderSystem->getParticleSystem().isAlive(mEffect);
	}

	void ParticlePreview::shutdown() noexcept
	{
		if (!mInitialised) return;
		try
		{
			auto& particles = mRenderSystem->getParticleSystem();
			if (mEffect && particles.isAlive(mEffect)) particles.destroyEffect(mEffect);
			particles.setStatisticsEnabled(false);
			mEffect = {};
			if (mEffectResource)
			{
				auto name = mEffectResource->getName();
				mEffectResource.reset();
				mResources->deleteResourceTree(name);
			}
			if (mScene) mScene->unload();
			mScene.reset();
			mCamera.reset();
			mRenderSystem->removeRenderPipeline(PreviewPipeline);
			mGraphResource.reset();
			if (mResources->getResource(PreviewGraph, true)) mResources->deleteResourceTree(PreviewGraph);
			mPresentationTexture.reset();
			mPresentationResource.reset();
			if (mResources->getResource(PreviewTarget, true)) mResources->deleteResourceTree(PreviewTarget);
		}
		catch (...) {}
		mInitialised = false;
	}
}
