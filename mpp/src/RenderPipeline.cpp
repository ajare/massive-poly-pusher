#include <algorithm>

#include "mpp/RenderPipeline.h"
#include "mpp/RenderSystem.h"
#include "mpp/RenderGraphExecutor.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{

	RenderPipeline::RenderPipeline(string const& name, RenderSystem* renderSystem, RenderPipelineOptions const& options)
		: mName(name)
		, mRenderSystem(renderSystem)
		, mOptions(options)
	{
		// The PBR preview path owns an HDR scene target. Legacy pipelines keep
		// their RGBA8 target and existing presentation behaviour.
		mPasses.push_back(make_shared<RenderPass>(renderSystem, mOptions.mode != RenderPipelineMode::LegacyForward));
	}

	RenderPipeline::~RenderPipeline()
	{
	}

	string const& RenderPipeline::getName() const
	{
		return mName;
	}

	RenderPipelineOptions const& RenderPipeline::getOptions() const
	{
		return mOptions;
	}

	void RenderPipeline::setExposure(float exposure)
	{
		mOptions.exposure = std::max(exposure, 0.0f);
	}

	void RenderPipeline::setToneMapOperator(PbrToneMapOperator toneMapOperator)
	{
		mOptions.toneMapOperator = toneMapOperator;
	}

	void RenderPipeline::setBloomOptions(BloomOptions const& bloomOptions)
	{
		mOptions.bloom = bloomOptions;
	}

	void RenderPipeline::ensureBloomTargets(size_t width, size_t height)
	{
		if (!mOptions.bloom.enabled)
		{
			mBloomExtractTarget.reset();
			mBloomPingTarget.reset();
			mBloomPongTarget.reset();
			mBloomCompositeTarget.reset();
			return;
		}

		auto needsCreate = [&](RenderTargetPtr const& target)
		{
			return !target || target->getWidth() != width || target->getHeight() != height;
		};
		if (!needsCreate(mBloomExtractTarget))
		{
			return;
		}

		RenderTextureOptions options;
		options.numAttachments = 1;
		options.colourType = TextureInternalType::Float;
		options.colourNormalised = false;
		options.colourBitSize = 16;
		options.params.minFilter = GL_LINEAR;
		options.params.magFilter = GL_LINEAR;
		options.params.wrap = GL_CLAMP_TO_EDGE;
		mBloomExtractTarget = mRenderSystem->createRenderTexture(mName + ".BloomExtract", width, height, options);
		mBloomPingTarget = mRenderSystem->createRenderTexture(mName + ".BloomPing", width, height, options);
		mBloomPongTarget = mRenderSystem->createRenderTexture(mName + ".BloomPong", width, height, options);
		mBloomCompositeTarget = mRenderSystem->createRenderTexture(mName + ".BloomComposite", width, height, options);
	}

	void RenderPipeline::setPbrEnvironment(PbrEnvironmentPtr environment)
	{
		mOptions.environment = std::move(environment);
	}

	void RenderPipeline::setShadowDomain(string const& shadowDomain)
	{
		mOptions.shadowDomain = shadowDomain;
	}

	RenderTargetPtr RenderPipeline::getOutputRenderTarget()
	{
		if (mOptions.bloom.enabled && mBloomCompositeTarget)
		{
			return mBloomCompositeTarget;
		}
		if (mPostEffects.empty())
		{
			return mPasses.back()->getRenderTarget();
		}
		else
		{
			return static_cast<PostEffect*>(mPostEffects.back().get())->getOuputRenderTarget();
		}
	}

	void RenderPipeline::resize(size_t width, size_t height)
	{
		for (auto const& pass : mPasses)
		{
			pass->resize(width, height);
		}
	}

	void RenderPipeline::addRenderPass(RenderPassPtr pass)
	{
		mPasses.push_back(pass);
	}

	void RenderPipeline::addPostEffect(ResourcePtr effect)
	{
		mPostEffects.push_back(effect);
	}

	void RenderPipeline::renderGraphPbr(ScenePtr scene, CameraPtr camera, vector<SceneModel3dPtr> const& models)
	{
		if (!mGraphTargets)
		{
			mGraphTargets = make_unique<RenderGraphTargets>(mRenderSystem);
			mGraphExecutor = make_unique<RenderGraphExecutor>(mRenderSystem);
		}

		RenderGraph graph;
		auto makeColour = [](GraphImageFormat format, bool external = false)
		{
			GraphImageDesc desc;
			desc.format = format;
			desc.usage = GraphImageUsage::ColourAttachment | (external ? GraphImageUsage::Presentation : GraphImageUsage::Sampled);
			desc.external = external;
			desc.transient = !external;
			desc.params.minFilter = GL_LINEAR;
			desc.params.magFilter = GL_LINEAR;
			desc.params.wrap = GL_CLAMP_TO_EDGE;
			return desc;
		};
		auto sceneHdr = graph.createImage("SceneHdr", makeColour(GraphImageFormat::Rgba16f));
		GraphImageDesc sceneDepthDesc;
		sceneDepthDesc.format = GraphImageFormat::Depth24;
		sceneDepthDesc.usage = GraphImageUsage::DepthAttachment;
		auto sceneDepth = graph.createImage("SceneDepth", sceneDepthDesc);

		GraphImageHandle shadowDepth;
		GraphImageHandle shadowDepthOutput;
		GraphPassHandle shadowPass;
		if (!mOptions.shadowDomain.empty())
		{
			GraphImageDesc shadowDesc;
			shadowDesc.format = GraphImageFormat::Depth24;
			shadowDesc.usage = GraphImageUsage::DepthAttachment | GraphImageUsage::Sampled;
			shadowDesc.external = true;
			shadowDesc.transient = false;
			auto const& shadowOptions = mRenderSystem->getShadowDomainOptions(mOptions.shadowDomain);
			shadowDesc.absoluteSize = glm::uvec2((uint32_t)shadowOptions.resolution);
			shadowDepth = graph.createImage("ShadowDepth", shadowDesc);
			shadowPass = graph.addPass("ShadowDepth");
			shadowDepthOutput = graph.writeDepth(shadowPass, shadowDepth, GraphLoadOp::Clear, GraphStoreOp::Store);
		}

		auto scenePass = graph.addPass("PbrScene");
		if (shadowDepthOutput.isValid()) graph.readSampled(scenePass, shadowDepthOutput);
		sceneHdr = graph.writeColour(scenePass, sceneHdr, GraphLoadOp::Clear, GraphStoreOp::Store,
			glm::vec4(scene->getClearColour().red, scene->getClearColour().green, scene->getClearColour().blue, scene->getClearColour().alpha));
		graph.writeDepth(scenePass, sceneDepth, GraphLoadOp::Clear, GraphStoreOp::DontCare);

		GraphImageHandle presentationTexture = sceneHdr;
		vector<GraphPassHandle> bloomPasses;
		vector<GraphImageHandle> bloomInputs;
		if (mOptions.bloom.enabled)
		{
			auto bloomExtract = graph.createImage("BloomExtract", makeColour(GraphImageFormat::Rgba16f));
			auto extractPass = graph.addPass("BloomExtract");
			graph.readSampled(extractPass, sceneHdr);
			bloomExtract = graph.writeColour(extractPass, bloomExtract);
			bloomPasses.push_back(extractPass);
			bloomInputs.push_back(sceneHdr);

			GraphImageHandle blurred = bloomExtract;
			for (uint32_t index = 0; index < mOptions.bloom.blurPasses; ++index)
			{
				auto ping = graph.createImage("BloomPing" + to_string(index), makeColour(GraphImageFormat::Rgba16f));
				auto pingPass = graph.addPass("BloomBlurHorizontal" + to_string(index));
				graph.readSampled(pingPass, blurred);
				ping = graph.writeColour(pingPass, ping);
				bloomPasses.push_back(pingPass);
				bloomInputs.push_back(blurred);

				auto pong = graph.createImage("BloomPong" + to_string(index), makeColour(GraphImageFormat::Rgba16f));
				auto pongPass = graph.addPass("BloomBlurVertical" + to_string(index));
				graph.readSampled(pongPass, ping);
				blurred = graph.writeColour(pongPass, pong);
				bloomPasses.push_back(pongPass);
				bloomInputs.push_back(ping);
			}
			auto composite = graph.createImage("BloomComposite", makeColour(GraphImageFormat::Rgba16f));
			auto compositePass = graph.addPass("BloomComposite");
			graph.readSampled(compositePass, sceneHdr);
			graph.readSampled(compositePass, blurred);
			presentationTexture = graph.writeColour(compositePass, composite);
			bloomPasses.push_back(compositePass);
			bloomInputs.push_back(blurred);
		}

		auto screen = graph.createImage("Presentation", makeColour(GraphImageFormat::Rgba8, true));
		auto toneMapPass = graph.addPass("ToneMapPresentation");
		graph.readSampled(toneMapPass, presentationTexture);
		graph.writeColour(toneMapPass, screen, GraphLoadOp::DontCare, GraphStoreOp::Store);

		auto const& viewport = scene->getViewport();
		auto plan = graph.buildAllocationPlan(glm::uvec2((uint32_t)viewport.width, (uint32_t)viewport.height));
		mGraphTargets->allocate(plan);
		mGraphTargets->bindImported(screen, mRenderSystem->getScreenRenderTarget());
		if (shadowDepth.isValid()) mGraphTargets->bindImported(shadowDepth, mRenderSystem->getShadowDomainDepthTarget(mOptions.shadowDomain));

		mGraphExecutor->clearPassCallbacks();
		if (shadowDepth.isValid())
		{
			mGraphExecutor->setPassCallback(shadowPass, [this, models](RenderGraphExecutionContext const&)
			{
				mRenderSystem->renderShadowDomain(mOptions.shadowDomain, models);
			});
		}
		mGraphExecutor->setPassCallback(scenePass, [this, scene, models, camera](RenderGraphExecutionContext const&)
		{
			if (!models.empty() && scene->show3dModels() && mPasses.back())
			{
				mPasses.back()->render(models, camera);
				mRenderSystem->flushVertexBuffers();
			}
		});
		for (size_t index = 0; index < bloomPasses.size(); ++index)
		{
			auto pass = bloomPasses[index];
			auto input = bloomInputs[index];
			if (index == 0)
			{
				mGraphExecutor->setPassCallback(pass, [this, input](RenderGraphExecutionContext const& context)
				{
					mRenderSystem->renderBloomExtract(static_cast<RenderTexture*>(context.getImage(input).get()), mOptions.bloom.threshold);
				});
			}
			else if (index + 1 == bloomPasses.size())
			{
				auto sceneInput = sceneHdr;
				mGraphExecutor->setPassCallback(pass, [this, sceneInput, input](RenderGraphExecutionContext const& context)
				{
					mRenderSystem->renderBloomCombine(static_cast<RenderTexture*>(context.getImage(sceneInput).get()), static_cast<RenderTexture*>(context.getImage(input).get()), mOptions.bloom.intensity);
				});
			}
			else
			{
				bool const horizontal = (index % 2) == 1;
				mGraphExecutor->setPassCallback(pass, [this, input, horizontal](RenderGraphExecutionContext const& context)
				{
					mRenderSystem->renderBloomBlur(static_cast<RenderTexture*>(context.getImage(input).get()), horizontal ? glm::vec2(1.0f, 0.0f) : glm::vec2(0.0f, 1.0f));
				});
			}
		}
		mGraphExecutor->setPassCallback(toneMapPass, [this, presentationTexture](RenderGraphExecutionContext const& context)
		{
			mRenderSystem->renderToneMappedFullscreenQuad(static_cast<RenderTexture*>(context.getImage(presentationTexture).get()), mOptions.exposure, mOptions.toneMapOperator == PbrToneMapOperator::Aces);
		});
		mGraphExecutor->execute(graph, *mGraphTargets, mRenderSystem->getCaps());
	}

	void RenderPipeline::render(ScenePtr scene, CameraPtr camera, glm::vec2 const& offset2d)
	{
		// Set viewport
		auto const& viewport = scene->getViewport();
		mRenderSystem->setViewport(viewport.x, viewport.y, (size_t)viewport.width, (size_t)viewport.height);

		// Lazily create shadow-domain resources for any participating pipeline.
		// The depth pass and shader consumption follow in later shadow milestones.
		if (!mOptions.shadowDomain.empty())
		{
			mRenderSystem->ensureShadowDomainResources(mOptions.shadowDomain);
		}

		// Scene passes
		mRenderSystem->setProjection3dPerspective(
			camera->getFov(),
			camera->getNearClipDistance(),
			camera->getFarClipDistance());
		

		auto const& models = scene->get3dModelsInView(camera);
		bool const graphPbr = mOptions.mode == RenderPipelineMode::GraphPbrForward;
		if (!graphPbr && !mOptions.shadowDomain.empty())
		{
			mRenderSystem->renderShadowDomain(mOptions.shadowDomain, models);
		}
		mRenderSystem->setActiveShadowDomain(mOptions.shadowDomain);

		map<string, ResourcePtr> pipelineSamplerOverrides;
		if (mOptions.mode == RenderPipelineMode::PbrForward || graphPbr)
		{
			mRenderSystem->setActivePbrEnvironment(mOptions.environment);
			if (mOptions.environment)
			{
				pipelineSamplerOverrides["PBR_IRRADIANCE_MAP"] = mOptions.environment->irradianceMap;
				pipelineSamplerOverrides["PBR_PREFILTERED_SPECULAR_MAP"] = mOptions.environment->prefilteredSpecularMap;
				pipelineSamplerOverrides["PBR_BRDF_LUT"] = mOptions.environment->brdfIntegrationLut;
			}
		}
		mRenderSystem->setActivePipelineSamplerOverrides(pipelineSamplerOverrides);
		if (graphPbr)
		{
			renderGraphPbr(scene, camera, models);
		}
		else
		{
			for (auto const& pass : mPasses)
			{
				// Start pass
				pass->bindRenderTarget();

				// Clear
				mRenderSystem->clearScreen(scene->getClearColour());

				// Render pass
				if (!models.empty() && scene->show3dModels())
				{
					pass->render(models, camera);

					// Flush
					mRenderSystem->flushVertexBuffers();
				}
			}
		}
		mRenderSystem->setActivePipelineSamplerOverrides({});
		mRenderSystem->setActiveShadowDomain("");
		if (mOptions.mode == RenderPipelineMode::PbrForward || graphPbr)
		{
			mRenderSystem->setActivePbrEnvironment(nullptr);
		}

		// Reset viewport
		mRenderSystem->resetViewport();

		if (!graphPbr)
		{
			// Pipeline image effects run after all material shading. PBR bloom is
			// therefore composed in HDR before tone mapping; legacy uses the same
			// effect sequence on its completed LDR scene target.
			mRenderSystem->setProjection2dOrthographic();
			mRenderSystem->resetTransform();
			auto sceneTexture = static_cast<RenderTexture*>(mPasses.back()->getRenderTarget().get());
			Texture* presentationTexture = sceneTexture;
			ensureBloomTargets(sceneTexture->getWidth(), sceneTexture->getHeight());
			if (mOptions.bloom.enabled)
			{
				mRenderSystem->setRenderTarget(mBloomExtractTarget);
				mRenderSystem->renderBloomExtract(sceneTexture, mOptions.bloom.threshold);

				Texture* blurredTexture = static_cast<RenderTexture*>(mBloomExtractTarget.get());
				for (uint32_t pass = 0; pass < mOptions.bloom.blurPasses; ++pass)
				{
					mRenderSystem->setRenderTarget(mBloomPingTarget);
					mRenderSystem->renderBloomBlur(blurredTexture, glm::vec2(1.0f, 0.0f));
					mRenderSystem->setRenderTarget(mBloomPongTarget);
					mRenderSystem->renderBloomBlur(static_cast<RenderTexture*>(mBloomPingTarget.get()), glm::vec2(0.0f, 1.0f));
					blurredTexture = static_cast<RenderTexture*>(mBloomPongTarget.get());
				}

				mRenderSystem->setRenderTarget(mBloomCompositeTarget);
				mRenderSystem->renderBloomCombine(sceneTexture, blurredTexture, mOptions.bloom.intensity);
				presentationTexture = static_cast<RenderTexture*>(mBloomCompositeTarget.get());
			}

			// Render to screen
			mRenderSystem->resetTransform();
			mRenderSystem->renderToScreen();
			mRenderSystem->clearScreen(scene->getClearColour());

			if (mOptions.mode == RenderPipelineMode::PbrForward)
			{
				mRenderSystem->renderToneMappedFullscreenQuad(presentationTexture, mOptions.exposure, mOptions.toneMapOperator == PbrToneMapOperator::Aces);
			}
			else
			{
				mRenderSystem->renderFullscreenQuad(presentationTexture, mpp::BlendMode::One, mpp::BlendMode::Zero);
			}
		}
		else
		{
			mRenderSystem->setProjection2dOrthographic();
			mRenderSystem->resetTransform();
		}

		// 2d models
		if (scene->show2dModels())
		{
			auto orderedModels = scene->get2dModelsInView();

			// Sort models.
			sort(orderedModels.begin(), orderedModels.end(), [](auto const& a, auto const& b)
			{
				return a.second < b.second;
			});

			mRenderSystem->pushModelMatrix();

			for (auto orderedModel: orderedModels)
			{
				auto model = orderedModel.first;

				auto const& origin = model->getOrigin();
				auto const& offset = model->getOffset();
				float angle = model->getAngle();
				float orbit = model->getOrbitAngle();
				auto const& scale = model->getScale();

				mRenderSystem->resetTransform();

				mRenderSystem->translateTransform2d(glm::vec2(-offset2d.x, -offset2d.y));

				// Scale and rotate object, then rotate around the origin, then move to world position.
				mRenderSystem->translateTransform2d(origin);
				mRenderSystem->rotateTransform2d(orbit);
				mRenderSystem->translateTransform2d(offset);
				mRenderSystem->rotateTransform2d(angle);
				mRenderSystem->scaleTransform2d(scale);

				model->render(camera);
			}

			mRenderSystem->popModelMatrix();

			// In case final model was batched and we're not rendering anything else this frame
			mRenderSystem->flushVertexBuffers();  
		}
	}
}