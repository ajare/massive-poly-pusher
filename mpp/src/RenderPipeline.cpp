#include <algorithm>
#include <atomic>
#include <glm/gtc/matrix_inverse.hpp>

#include "mpp/RenderPipeline.h"
#include "mpp/ResourceManager.h"
#include "mpp/RenderSystem.h"
#include "mpp/RenderGraphExecutor.h"
#include "mpp/RenderGraphBuiltInPasses.h"
#include "mpp/RenderGraphTemplate.h"
#include "mpp/RenderGraphImportRegistry.h"
#include "mpp/GLErrorCheck.h"
#include "mpp/GpuDebugScope.h"
#include "mpp/MppException.h"
#include "mpp/Material.h"
#include "mpp/Program.h"

using namespace std;

namespace mpp
{
	namespace
	{
		atomic<uint64_t> nextFlowGeneration{ 1 };

		char const* pipelineModeName(RenderPipelineMode mode)
		{
			switch (mode)
			{
			case RenderPipelineMode::LegacyForward: return "LegacyForward";
			case RenderPipelineMode::PbrForward: return "PbrForward";
			case RenderPipelineMode::GraphPbrForward: return "GraphPbrForward";
			case RenderPipelineMode::XmlGraphPbrForward: return "XmlGraphPbrForward";
			case RenderPipelineMode::GraphLegacyForward: return "GraphLegacyForward";
			default: return "Unknown";
			}
		}

		bool sceneProgramsSupportOutputs(vector<SceneModel3dPtr> const& models, size_t requiredCount)
		{
			for (auto const& sceneModel : models)
			{
				auto model = static_cast<Model*>(sceneModel->getModel().get());
				for (int meshIndex = 0; meshIndex < model->getNumMeshes(); ++meshIndex)
				{
					auto material = static_cast<Material*>(model->getMesh(meshIndex)->getMaterial().get());
					auto program = static_cast<Program*>(material->getProgram().get());
					string diagnostic;
					if (!program || !program->validateFragmentOutputLocations(requiredCount, diagnostic)) return false;
				}
			}
			return true;
		}
	}


	RenderPipeline::RenderPipeline(string const& name, RenderSystem* renderSystem, RenderPipelineOptions const& options)
		: mName(name)
		, mRenderSystem(renderSystem)
		, mOptions(options)
	{
		mFlowGeneration = nextFlowGeneration.fetch_add(1, memory_order_relaxed);
		if(!mOptions.outputs.empty())mOutputProcessor=make_unique<RenderOutputProcessor>(renderSystem,mName);
		// The PBR preview path owns an HDR scene target. Legacy pipelines keep
		// their RGBA8 target and existing presentation behaviour.
		bool const pbr = mOptions.mode == RenderPipelineMode::PbrForward || mOptions.mode == RenderPipelineMode::GraphPbrForward || mOptions.mode == RenderPipelineMode::XmlGraphPbrForward;
		mPasses.push_back(make_shared<RenderPass>(renderSystem, pbr, mName + (pbr ? ".SceneHDR" : ".SceneLDR")));
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

	void RenderPipeline::setGraphPassDebugOptions(GraphPassDebugOptions const& graphPasses)
	{
		mOptions.graphPasses = graphPasses;
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

	RenderTargetPtr RenderPipeline::getGraphImageRenderTarget(GraphImageHandle image) const
	{
		return mGraphTargets?mGraphTargets->get(image):nullptr;
	}

	vector<GraphPassExecutionStats> const& RenderPipeline::getLastGraphExecutionStats() const
	{
		static vector<GraphPassExecutionStats> const empty;return mGraphExecutor?mGraphExecutor->getLastExecutionStats():empty;
	}

	vector<GraphPassHandle> const& RenderPipeline::getLastGraphExecutionOrder() const
	{
		static vector<GraphPassHandle> const empty;return mGraphExecutor?mGraphExecutor->getLastExecutionOrder():empty;
	}

	void RenderPipeline::setFlowTelemetryEnabled(bool enabled)
	{
		if(mFlowTelemetryEnabled==enabled)return;mFlowTelemetryEnabled=enabled;mLastFlowSnapshot.reset();
	}

	bool RenderPipeline::isFlowTelemetryEnabled() const{return mFlowTelemetryEnabled;}
	RenderPipelineFlowSnapshotPtr RenderPipeline::getLastFlowSnapshot() const{return mLastFlowSnapshot;}

	void RenderPipeline::publishFlowSnapshot()
	{
		if(!mFlowTelemetryEnabled||!mGraphExecutor)return;auto snapshot=make_shared<RenderPipelineFlowSnapshot>();snapshot->frameSerial=mRenderSystem->getFrameSerial();snapshot->pipelineGeneration=mFlowGeneration;snapshot->actualPassOrder=mGraphExecutor->getLastExecutionOrder();mLastFlowSnapshot=std::move(snapshot);
	}

	uint64_t RenderPipeline::getOutputGeneration() const{return mOutputProcessor?mOutputProcessor->getGeneration():0;}
	vector<RenderPipelineOutputPlan> const& RenderPipeline::getOutputPlans() const{static vector<RenderPipelineOutputPlan> const empty;return mOutputProcessor?mOutputProcessor->getPlans():empty;}
	void RenderPipeline::prepareOutputs(RenderGraph const& graph,map<string,RenderTargetPtr> const& destinations){if(mOutputProcessor)mOutputProcessor->rebuild(mOptions.outputs,graph,destinations,mRenderSystem->getOptions().antiAliasing);}

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

	void RenderPipeline::renderGraphForward(ScenePtr scene, CameraPtr camera, vector<SceneModel3dPtr> const& models, bool pbr)
	{
		GpuDebugScope graphScope("Pipeline " + mName + ": RenderGraph");
		auto outputAntiAliasing=mOptions.outputs.empty()?AntiAliasingDefaults{}:resolveAntiAliasing(mRenderSystem->getOptions().antiAliasing,mOptions.outputs.front().antiAliasing);uint32_t physicalSamples=antiAliasingSampleCount(outputAntiAliasing.msaa);std::optional<TaaFrameContext> taaFrame;struct JitterReset{Camera* camera{};~JitterReset(){if(camera)camera->setProjectionJitter({0,0});}}jitterReset;
		if(outputAntiAliasing.taa){auto const& viewport=scene->getViewport();auto rasterWidth=ssaaDimension((uint32_t)viewport.width,outputAntiAliasing.ssaa),rasterHeight=ssaaDimension((uint32_t)viewport.height,outputAntiAliasing.ssaa);auto jitter=taaHaltonJitter(mTaaSequenceIndex++);camera->setProjectionJitter({2.0f*jitter.x/(float)rasterWidth,2.0f*jitter.y/(float)rasterHeight});jitterReset.camera=camera.get();auto direction=camera->getDirection();auto frameSerial=mRenderSystem->getFrameSerial();bool discontinuity=!mTaaCameraValid||camera->getCutRevision()!=mLastCameraCutRevision||(mLastTaaFrameSerial&&frameSerial!=mLastTaaFrameSerial+1)||glm::distance(camera->getPosition(),mLastCameraPosition)>std::max(1.0f,std::min(10.0f,camera->getFarClipDistance()*0.01f))||glm::dot(direction,mLastCameraDirection)<0.8f||std::abs(camera->getFov()-mLastCameraFov)>10.0f||std::abs(camera->getAspectRatio()-mLastCameraAspect)>0.001f||std::abs(camera->getNearClipDistance()-mLastCameraNear)>0.001f||std::abs(camera->getFarClipDistance()-mLastCameraFar)>0.01f;auto viewProjection=camera->getProjectionTransform()*camera->getViewTransform();taaFrame=TaaFrameContext{viewProjection,glm::inverse(viewProjection),frameSerial,discontinuity};mTaaCameraValid=true;mLastTaaFrameSerial=frameSerial;mLastCameraCutRevision=camera->getCutRevision();mLastCameraPosition=camera->getPosition();mLastCameraDirection=direction;mLastCameraFov=camera->getFov();mLastCameraAspect=camera->getAspectRatio();mLastCameraNear=camera->getNearClipDistance();mLastCameraFar=camera->getFarClipDistance();}
		if (!mGraphTargets)
		{
			mGraphTargets = make_unique<RenderGraphTargets>(mRenderSystem);
			mGraphExecutor = make_unique<RenderGraphExecutor>(mRenderSystem);
			registerBuiltInRenderGraphPasses(mGraphPassFactories);
			mGraphExecutor->setPassFactoryRegistry(&mGraphPassFactories);
		}

		if (mOptions.graphTemplate)
		{
			ResourcePtr selectedTemplate = mOptions.graphTemplate;
			bool const useXmlMrt = pbr && mOptions.graphTemplateMrt && mOptions.bloom.enabled && mOptions.bloom.useMrtEmissiveMask &&
				mRenderSystem->getCaps().maxDrawBuffers >= 2 && mRenderSystem->getCaps().maxColourAttachments >= 2 &&
				sceneProgramsSupportOutputs(models, 2);
			if (useXmlMrt) selectedTemplate = mOptions.graphTemplateMrt;
			auto templateResource = dynamic_cast<RenderGraphTemplate*>(selectedTemplate.get());
			if (!templateResource) THROW_MPP("XmlGraphPbrForward requires a RenderGraph resource.", __LINE__, __FILE__, __func__);
			templateResource->create();
			templateResource->load();
			auto const& graph = templateResource->getGraph();
			if (!graph) THROW_MPP("XmlGraphPbrForward graph template is empty.", __LINE__, __FILE__, __func__);
			auto const& viewport = scene->getViewport();
			mGraphTargets->allocatePhysical(graph->buildAllocationPlan(glm::uvec2(ssaaDimension((uint32_t)viewport.width,outputAntiAliasing.ssaa),ssaaDimension((uint32_t)viewport.height,outputAntiAliasing.ssaa))),physicalSamples);
			RenderGraphImportRegistry imports;
			for(auto const& entry:mOptions.graphImports)imports.registerImport(entry.first,entry.second);
			if(!imports.findImport("screen"))imports.registerImport("screen", mRenderSystem->getScreenRenderTarget());
			if (!mOptions.shadowDomain.empty()&&!imports.findImport("shadowDepth")) imports.registerImport("shadowDepth", mRenderSystem->getShadowDomainDepthTarget(mOptions.shadowDomain));
			mGraphTargets->bindImports(*graph, imports);
			struct PreparedOutput{string name;GraphImageHandle image;GraphImageHandle depth;RenderTargetPtr destination;RenderTargetPtr source;bool external;};vector<PreparedOutput> preparedOutputs;
			if(mOutputProcessor)
			{
				map<string,RenderTargetPtr> destinations;
				for(auto const& output:mOptions.outputs)
				{
					GraphImageHandle handle;GraphImageInfo info;for(uint32_t id=0;id<graph->getImageCount();++id){auto candidate=graph->getImageInfo({id,0});if(candidate.name==output.image){handle={id,0};info=candidate;break;}}if(!handle.isValid())THROW_MPP("Named output '"+output.name+"' references an unknown graph image.",__LINE__,__FILE__,__func__);for(uint32_t pass=0;pass<graph->getPassCount();++pass){auto const& passInfo=graph->getPassInfo({pass});for(auto const& attachment:passInfo.colourOutputs)if(attachment.image.id==handle.id&&attachment.image.version>handle.version)handle=attachment.image;}
					GraphImageHandle depthHandle;if(!output.taaDepth.empty()){for(uint32_t id=0;id<graph->getImageCount();++id)if(graph->getImageInfo({id,0}).name==output.taaDepth){depthHandle={id,0};break;}for(uint32_t pass=0;pass<graph->getPassCount();++pass)for(auto const& attachment:graph->getPassInfo({pass}).depthOutputs)if(attachment.image.id==depthHandle.id&&attachment.image.version>depthHandle.version)depthHandle=attachment.image;}auto destination=info.desc.external?imports.findImport(info.importName):mGraphTargets->get(handle);if(!destination)THROW_MPP("Named output '"+output.name+"' has no render target.",__LINE__,__FILE__,__func__);destinations.emplace(output.name,destination);preparedOutputs.push_back({output.name,handle,depthHandle,destination,{},info.desc.external});
				}
				mOutputProcessor->rebuild(mOptions.outputs,*graph,destinations,mRenderSystem->getOptions().antiAliasing);
				map<uint32_t,RenderTargetPtr> externalSources;for(auto& output:preparedOutputs)if(output.external){auto [found,inserted]=externalSources.emplace(output.image.id,mOutputProcessor->getInput(output.name));output.source=found->second;if(inserted)mGraphTargets->bindImported(output.image,output.source);}
			}
			// XML supplies defaults; current pipeline controls override dynamic
			// per-frame values without recompiling or mutating the template.
			for (uint32_t id = 0; id < graph->getPassCount(); ++id)
			{
				GraphPassHandle pass{ id };
				auto const info = graph->getPassInfo(pass);
				UniformCollection overrides;
				if (info.name == "BloomExtract")
				{
					overrides.setUniform("THRESHOLD", mOptions.bloom.threshold);
					mGraphExecutor->setPassParameterOverrides(pass, overrides);
				}
				else if (info.name == "BloomComposite")
				{
					overrides.setUniform("INTENSITY", mOptions.bloom.intensity);
					mGraphExecutor->setPassParameterOverrides(pass, overrides);
				}
				else if (info.name == "ToneMapPresentation")
				{
					overrides.setUniform("EXPOSURE", mOptions.exposure);
					overrides.setUniform("TONE_MAP_OPERATOR", (int32_t)(mOptions.toneMapOperator == PbrToneMapOperator::Aces ? 1 : 0));
					mGraphExecutor->setPassParameterOverrides(pass, overrides);
				}
			}
			RenderGraphFrameContext frameContext{ mRenderSystem, scene, camera, models, &mOptions, mPasses.back() };
			mGraphExecutor->setFrameContext(&frameContext);
			mGraphExecutor->execute(*templateResource, *mGraphTargets, mRenderSystem->getCaps());
			mGraphExecutor->setFrameContext(nullptr);
			for(auto const& output:preparedOutputs){auto depth=output.depth.isValid()?mGraphTargets->get(output.depth):output.destination;mOutputProcessor->present(output.name,output.destination,output.external?output.source:mGraphTargets->get(output.image),depth,taaFrame?&*taaFrame:nullptr);}
			publishFlowSnapshot();
			return;
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
		bool const useMrtEmissiveMask = pbr && mOptions.bloom.enabled && mOptions.bloom.useMrtEmissiveMask &&
			mRenderSystem->getCaps().maxDrawBuffers >= 2 && mRenderSystem->getCaps().maxColourAttachments >= 2 &&
			sceneProgramsSupportOutputs(models, 2);
		auto sceneHdr = graph.createImage(pbr ? "SceneHdr" : "SceneLdr", makeColour(pbr ? GraphImageFormat::Rgba16f : GraphImageFormat::Rgba8));
		GraphImageHandle bloomMask;
		if (useMrtEmissiveMask)
		{
			bloomMask = graph.createImage("BloomMaskHdr", makeColour(GraphImageFormat::Rgba16f));
		}
		GraphImageDesc sceneDepthDesc;
		sceneDepthDesc.format = GraphImageFormat::Depth24;
		sceneDepthDesc.usage = GraphImageUsage::DepthAttachment | (outputAntiAliasing.taa ? GraphImageUsage::Sampled : GraphImageUsage::None);
		auto sceneDepth = graph.createImage("SceneDepth", sceneDepthDesc);

		GraphImageHandle shadowDepth;
		GraphImageHandle shadowDepthOutput;
		GraphPassHandle shadowPass;
		if (!mOptions.shadowDomain.empty() && mOptions.graphPasses.shadow)
		{
			GraphImageDesc shadowDesc;
			shadowDesc.format = GraphImageFormat::Depth24;
			shadowDesc.usage = GraphImageUsage::DepthAttachment | GraphImageUsage::Sampled;
			shadowDesc.external = true;
			shadowDesc.transient = false;
			auto const& shadowOptions = mRenderSystem->getShadowDomainOptions(mOptions.shadowDomain);
			shadowDesc.absoluteSize = glm::uvec2((uint32_t)shadowOptions.resolution);
			shadowDepth = graph.createImage("ShadowDepth", shadowDesc);
			shadowPass = graph.addPass("ShadowDepth", GraphPassType::Scene);
			shadowDepthOutput = graph.writeDepth(shadowPass, shadowDepth, GraphLoadOp::Clear, GraphStoreOp::Store);
		}

		auto scenePass = graph.addPass(pbr ? "PbrScene" : "LegacyScene", GraphPassType::Scene);
		if (shadowDepthOutput.isValid()) graph.readSampled(scenePass, shadowDepthOutput);
		sceneHdr = graph.writeColour(scenePass, sceneHdr, GraphLoadOp::Clear, GraphStoreOp::Store,
			glm::vec4(scene->getClearColour().red, scene->getClearColour().green, scene->getClearColour().blue, scene->getClearColour().alpha));
		if (useMrtEmissiveMask) bloomMask = graph.writeColour(scenePass, bloomMask, GraphLoadOp::Clear, GraphStoreOp::Store);
		sceneDepth=graph.writeDepth(scenePass,sceneDepth,GraphLoadOp::Clear,outputAntiAliasing.taa?GraphStoreOp::Store:GraphStoreOp::DontCare);

		GraphImageHandle presentationTexture = sceneHdr;
		enum class BloomGraphStep { Extract, Horizontal, Vertical, Composite };
		vector<GraphPassHandle> bloomPasses;
		vector<GraphImageHandle> bloomInputs;
		vector<BloomGraphStep> bloomSteps;
		if (mOptions.bloom.enabled && mOptions.graphPasses.bloom)
		{
			GraphImageHandle blurred = bloomMask;
			if (!useMrtEmissiveMask)
			{
				auto bloomExtract = graph.createImage("BloomExtract", makeColour(GraphImageFormat::Rgba16f));
				auto extractPass = graph.addPass("BloomExtract", GraphPassType::Fullscreen);
				graph.readSampled(extractPass, sceneHdr);
				blurred = graph.writeColour(extractPass, bloomExtract);
				bloomPasses.push_back(extractPass);
				bloomInputs.push_back(sceneHdr);
				bloomSteps.push_back(BloomGraphStep::Extract);
			}
			for (uint32_t index = 0; index < mOptions.bloom.blurPasses; ++index)
			{
				auto ping = graph.createImage("BloomPing" + to_string(index), makeColour(GraphImageFormat::Rgba16f));
				auto pingPass = graph.addPass("BloomBlurHorizontal" + to_string(index), GraphPassType::Fullscreen);
				graph.readSampled(pingPass, blurred);
				ping = graph.writeColour(pingPass, ping);
				bloomPasses.push_back(pingPass);
				bloomInputs.push_back(blurred);
				bloomSteps.push_back(BloomGraphStep::Horizontal);

				auto pong = graph.createImage("BloomPong" + to_string(index), makeColour(GraphImageFormat::Rgba16f));
				auto pongPass = graph.addPass("BloomBlurVertical" + to_string(index), GraphPassType::Fullscreen);
				graph.readSampled(pongPass, ping);
				blurred = graph.writeColour(pongPass, pong);
				bloomPasses.push_back(pongPass);
				bloomInputs.push_back(ping);
				bloomSteps.push_back(BloomGraphStep::Vertical);
			}
			auto composite = graph.createImage("BloomComposite", makeColour(GraphImageFormat::Rgba16f));
			auto compositePass = graph.addPass("BloomComposite", GraphPassType::Fullscreen);
			graph.readSampled(compositePass, sceneHdr);
			graph.readSampled(compositePass, blurred);
			presentationTexture = graph.writeColour(compositePass, composite);
			bloomPasses.push_back(compositePass);
			bloomInputs.push_back(blurred);
			bloomSteps.push_back(BloomGraphStep::Composite);
		}

		auto screen = graph.createImage("Presentation", makeColour(GraphImageFormat::Rgba8, true));
		auto toneMapPass = graph.addPass("ToneMapPresentation", GraphPassType::Present);
		graph.readSampled(toneMapPass, presentationTexture);
		graph.writeColour(toneMapPass, screen, GraphLoadOp::Clear, GraphStoreOp::Store, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

		auto const& viewport = scene->getViewport();
		auto plan = graph.buildAllocationPlan(glm::uvec2(ssaaDimension((uint32_t)viewport.width,outputAntiAliasing.ssaa),ssaaDimension((uint32_t)viewport.height,outputAntiAliasing.ssaa)));
		mGraphTargets->allocatePhysical(plan,physicalSamples);
		mGraphTargets->bindImported(screen, mRenderSystem->getScreenRenderTarget());
		struct DynamicPreparedOutput{string name;GraphImageHandle image;GraphImageHandle depth;RenderTargetPtr destination;RenderTargetPtr source;bool external;};vector<DynamicPreparedOutput> dynamicOutputs;
		if(mOutputProcessor)
		{
			map<string,RenderTargetPtr> destinations;for(auto const& output:mOptions.outputs){GraphImageHandle handle;GraphImageInfo info;for(uint32_t id=0;id<graph.getImageCount();++id){auto candidate=graph.getImageInfo({id,0});if(candidate.name==output.image){handle={id,0};info=candidate;break;}}if(!handle.isValid())THROW_MPP("Named output '"+output.name+"' references an unknown generated graph image.",__LINE__,__FILE__,__func__);for(uint32_t pass=0;pass<graph.getPassCount();++pass){auto const& passInfo=graph.getPassInfo({pass});for(auto const& attachment:passInfo.colourOutputs)if(attachment.image.id==handle.id&&attachment.image.version>handle.version)handle=attachment.image;}auto destination=info.desc.external?mRenderSystem->getScreenRenderTarget():mGraphTargets->get(handle);destinations.emplace(output.name,destination);GraphImageHandle depthHandle;if(!output.taaDepth.empty()){for(uint32_t id=0;id<graph.getImageCount();++id)if(graph.getImageInfo({id,0}).name==output.taaDepth){depthHandle={id,0};break;}for(uint32_t pass=0;pass<graph.getPassCount();++pass)for(auto const& attachment:graph.getPassInfo({pass}).depthOutputs)if(attachment.image.id==depthHandle.id&&attachment.image.version>depthHandle.version)depthHandle=attachment.image;}dynamicOutputs.push_back({output.name,handle,depthHandle,destination,{},info.desc.external});}mOutputProcessor->rebuild(mOptions.outputs,graph,destinations,mRenderSystem->getOptions().antiAliasing);map<uint32_t,RenderTargetPtr> externalSources;for(auto& output:dynamicOutputs)if(output.external){auto [found,inserted]=externalSources.emplace(output.image.id,mOutputProcessor->getInput(output.name));output.source=found->second;if(inserted)mGraphTargets->bindImported(output.image,output.source);}
		}
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
			if (mOptions.graphPasses.scene && !models.empty() && scene->show3dModels() && mPasses.back())
			{
				mPasses.back()->render(models, camera);
				mRenderSystem->flushVertexBuffers();
			}
		});
		for (size_t index = 0; index < bloomPasses.size(); ++index)
		{
			auto pass = bloomPasses[index];
			auto input = bloomInputs[index];
			switch (bloomSteps[index])
			{
			case BloomGraphStep::Extract:
				mGraphExecutor->setPassCallback(pass, [this, input](RenderGraphExecutionContext const& context)
				{
					mRenderSystem->renderBloomExtract(static_cast<RenderTexture*>(context.getImage(input).get()), mOptions.bloom.threshold);
				});
				break;
			case BloomGraphStep::Horizontal:
			case BloomGraphStep::Vertical:
			{
				bool const horizontal = bloomSteps[index] == BloomGraphStep::Horizontal;
				mGraphExecutor->setPassCallback(pass, [this, input, horizontal](RenderGraphExecutionContext const& context)
				{
					mRenderSystem->renderBloomBlur(static_cast<RenderTexture*>(context.getImage(input).get()), horizontal ? glm::vec2(1.0f, 0.0f) : glm::vec2(0.0f, 1.0f));
				});
				break;
			}
			case BloomGraphStep::Composite:
			{
				auto sceneInput = sceneHdr;
				mGraphExecutor->setPassCallback(pass, [this, sceneInput, input](RenderGraphExecutionContext const& context)
				{
					mRenderSystem->renderBloomCombine(static_cast<RenderTexture*>(context.getImage(sceneInput).get()), static_cast<RenderTexture*>(context.getImage(input).get()), mOptions.bloom.intensity);
				});
				break;
			}
			}
		}
		mGraphExecutor->setPassCallback(toneMapPass, [this, presentationTexture, pbr](RenderGraphExecutionContext const& context)
		{
			if (!mOptions.graphPasses.presentation) return;
			auto texture = static_cast<RenderTexture*>(context.getImage(presentationTexture).get());
			if (pbr)
			{
				mRenderSystem->renderToneMappedFullscreenQuad(texture, mOptions.exposure, mOptions.toneMapOperator == PbrToneMapOperator::Aces);
			}
			else
			{
				mRenderSystem->renderFullscreenQuad(texture, BlendMode::One, BlendMode::Zero);
			}
		});
		RenderGraphFrameContext frameContext;
		frameContext.renderSystem = mRenderSystem;
		frameContext.scene = scene;
		frameContext.camera = camera;
		frameContext.visibleModels = models;
		frameContext.pipelineOptions = &mOptions;
		frameContext.sceneRenderPass = mPasses.back();
		mGraphExecutor->setFrameContext(&frameContext);
		mGraphExecutor->execute(graph, *mGraphTargets, mRenderSystem->getCaps());
		mGraphExecutor->setFrameContext(nullptr);
		for(auto const& output:dynamicOutputs){auto depth=output.depth.isValid()?mGraphTargets->get(output.depth):output.destination;mOutputProcessor->present(output.name,output.destination,output.external?output.source:mGraphTargets->get(output.image),depth,taaFrame?&*taaFrame:nullptr);}
		publishFlowSnapshot();
	}

	void RenderPipeline::render(ScenePtr scene, CameraPtr camera, glm::vec2 const& offset2d)
	{
		GpuDebugScope pipelineScope("RenderPipeline: " + mName + " [" + pipelineModeName(mOptions.mode) + "]");
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
		bool const graphPbr = mOptions.mode == RenderPipelineMode::GraphPbrForward || mOptions.mode == RenderPipelineMode::XmlGraphPbrForward;
		bool const graphLegacy = mOptions.mode == RenderPipelineMode::GraphLegacyForward;
		bool const graphForward = graphPbr || graphLegacy;
		if ((mOptions.mode == RenderPipelineMode::PbrForward || graphPbr) && scene->ownsPbrLights()) mRenderSystem->setPbrLights(scene->getPbrLights());
		if (!graphForward && !mOptions.shadowDomain.empty())
		{
			GpuDebugScope shadowScope("Pass: ShadowDomain [" + mOptions.shadowDomain + "]");
			mRenderSystem->renderShadowDomain(mOptions.shadowDomain, models);
		}
		mRenderSystem->setActiveShadowDomain(mOptions.shadowDomain);

		map<string, ResourcePtr> pipelineSamplerOverrides;
		if (mOptions.mode == RenderPipelineMode::PbrForward || graphPbr)
		{
			mRenderSystem->setActivePbrEnvironment(mOptions.environment);
			auto const cubeFallback = mRenderSystem->getResourceManager()->getResource("__mpp_tex_pbr_ibl_cube__");
			auto const brdfFallback = mRenderSystem->getResourceManager()->getResource("__mpp_tex_pbr_brdf_lut__");
			pipelineSamplerOverrides["PBR_IRRADIANCE_MAP"] = mOptions.environment && mOptions.environment->irradianceMap ? mOptions.environment->irradianceMap : cubeFallback;
			pipelineSamplerOverrides["PBR_PREFILTERED_SPECULAR_MAP"] = mOptions.environment && mOptions.environment->prefilteredSpecularMap ? mOptions.environment->prefilteredSpecularMap : cubeFallback;
			pipelineSamplerOverrides["PBR_BRDF_LUT"] = mOptions.environment && mOptions.environment->brdfIntegrationLut ? mOptions.environment->brdfIntegrationLut : brdfFallback;
			if ((!mOptions.environment || !mOptions.environment->irradianceMap || !mOptions.environment->prefilteredSpecularMap || !mOptions.environment->brdfIntegrationLut) && !mWarnedMissingPbrEnvironment)
			{
				mRenderSystem->warnMessage("PBR pipeline '" + mName + "' has no complete environment; neutral IBL fallbacks are active.");
				mWarnedMissingPbrEnvironment = true;
			}
		}
		mRenderSystem->setActivePipelineSamplerOverrides(pipelineSamplerOverrides);
		if (graphForward)
		{
			renderGraphForward(scene, camera, models, graphPbr);
		}
		else
		{
			for (size_t passIndex = 0; passIndex < mPasses.size(); ++passIndex)
			{
				auto const& pass = mPasses[passIndex];
				GpuDebugScope sceneScope("Pass: Scene " + to_string(passIndex) + (mOptions.mode == RenderPipelineMode::PbrForward ? " [HDR PBR]" : " [LDR Legacy]"));
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

		// Graph execution restores the target that was active before the graph.
		// Explicitly reactivate the screen before UI/2D rendering; otherwise the
		// next frame can draw UI into a transient graph target and flicker.
		if (graphForward)
		{
			mRenderSystem->renderToScreen();
			// With presentation disabled no graph pass writes the double-buffered
			// backbuffer. Clear it explicitly so alternating stale buffers do not
			// look like graph/UI flicker during pass isolation.
			if (!mOptions.graphPasses.presentation)
			{
				mRenderSystem->clearScreen(scene->getClearColour());
			}
		}

		// Reset viewport
		mRenderSystem->resetViewport();

		if (!graphForward)
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
				GpuDebugScope bloomScope(string("Post: Bloom [") + (mOptions.mode == RenderPipelineMode::PbrForward ? "HDR" : "LDR") + "]");
				{
					GpuDebugScope extractScope("Bloom: Extract");
					mRenderSystem->setRenderTarget(mBloomExtractTarget);
					mRenderSystem->renderBloomExtract(sceneTexture, mOptions.bloom.threshold);
				}

				Texture* blurredTexture = static_cast<RenderTexture*>(mBloomExtractTarget.get());
				for (uint32_t pass = 0; pass < mOptions.bloom.blurPasses; ++pass)
				{
					{
						GpuDebugScope blurScope("Bloom: Blur Horizontal " + to_string(pass));
						mRenderSystem->setRenderTarget(mBloomPingTarget);
						mRenderSystem->renderBloomBlur(blurredTexture, glm::vec2(1.0f, 0.0f));
					}
					{
						GpuDebugScope blurScope("Bloom: Blur Vertical " + to_string(pass));
						mRenderSystem->setRenderTarget(mBloomPongTarget);
						mRenderSystem->renderBloomBlur(static_cast<RenderTexture*>(mBloomPingTarget.get()), glm::vec2(0.0f, 1.0f));
					}
					blurredTexture = static_cast<RenderTexture*>(mBloomPongTarget.get());
				}

				{
					GpuDebugScope compositeScope("Bloom: Composite");
					mRenderSystem->setRenderTarget(mBloomCompositeTarget);
					mRenderSystem->renderBloomCombine(sceneTexture, blurredTexture, mOptions.bloom.intensity);
				}
				presentationTexture = static_cast<RenderTexture*>(mBloomCompositeTarget.get());
			}

			// Render to screen
			mRenderSystem->resetTransform();
			mRenderSystem->renderToScreen();
			mRenderSystem->clearScreen(scene->getClearColour());

			if (mOptions.mode == RenderPipelineMode::PbrForward)
			{
				GpuDebugScope presentationScope("Post: Tone Map + Presentation [" + string(mOptions.toneMapOperator == PbrToneMapOperator::Aces ? "ACES" : "Reinhard") + "]");
				mRenderSystem->renderToneMappedFullscreenQuad(presentationTexture, mOptions.exposure, mOptions.toneMapOperator == PbrToneMapOperator::Aces);
			}
			else
			{
				GpuDebugScope presentationScope("Post: Presentation [LDR]");
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
			GpuDebugScope overlayScope("Pass: 2D Overlay");
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