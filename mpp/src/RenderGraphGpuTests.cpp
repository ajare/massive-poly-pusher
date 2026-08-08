#include <glew/glew.h>

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include "mpp/RenderGraphGpuTests.h"
#include "mpp/Camera.h"
#include "mpp/RenderGraph.h"
#include "mpp/RenderGraphExecutor.h"
#include "mpp/RenderGraphTargets.h"
#include "mpp/RenderOutputProcessor.h"
#include "mpp/RenderSystem.h"
#include "mpp/RenderTexture.h"
#include "mpp/GLErrorCheck.h"
#include "mpp/MppException.h"

namespace mpp
{
	namespace
	{
		std::array<uint8_t, 4> readFirstPixel(RenderTargetPtr const& target, uint32_t mipLevel = 0)
		{
			auto texture = dynamic_cast<RenderTexture*>(target.get());
			if (!texture) return { 0, 0, 0, 0 };
			size_t width = std::max<size_t>(1, texture->getWidth() >> mipLevel);
			size_t height = std::max<size_t>(1, texture->getHeight() >> mipLevel);
			std::vector<uint8_t> pixels(width * height * 4);
			GL_CHECK(glBindTexture(GL_TEXTURE_2D, texture->getColourAttachmentId(0)));
			GL_CHECK(glGetTexImage(GL_TEXTURE_2D, (GLint)mipLevel, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data()));
			GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
			return { pixels[0], pixels[1], pixels[2], pixels[3] };
		}

		float readFirstDepth(RenderTargetPtr const& target)
		{
			auto texture=dynamic_cast<RenderTexture*>(target.get());if(!texture||!texture->getDepthTextureId())return -1.0f;std::vector<float> values(texture->getWidth()*texture->getHeight());GL_CHECK(glBindTexture(GL_TEXTURE_2D,texture->getDepthTextureId()));GL_CHECK(glGetTexImage(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,GL_FLOAT,values.data()));GL_CHECK(glBindTexture(GL_TEXTURE_2D,0));return values.empty()?-1.0f:values.front();
		}

		bool nearColour(std::array<uint8_t, 4> const& pixel, std::array<uint8_t, 4> const& expected)
		{
			for (size_t i = 0; i < 4; ++i)
				if (std::abs((int)pixel[i] - (int)expected[i]) > 1) return false;
			return true;
		}

		std::vector<uint8_t> readPixels(RenderTargetPtr const& target)
		{
			auto texture = dynamic_cast<RenderTexture*>(target.get());
			if (!texture) return {};
			std::vector<uint8_t> pixels(texture->getWidth() * texture->getHeight() * 4);
			GL_CHECK(glBindTexture(GL_TEXTURE_2D, texture->getColourAttachmentId(0)));
			GL_CHECK(glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data()));
			GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
			return pixels;
		}

		bool containsVisiblePixel(RenderTargetPtr const& target)
		{
			auto pixels = readPixels(target);
			for (size_t index = 0; index < pixels.size(); index += 4)
			{
				if (pixels[index] || pixels[index + 1] || pixels[index + 2]) return true;
			}
			return false;
		}

		uint8_t maximumRed(RenderTargetPtr const& target)
		{
			auto pixels = readPixels(target);
			uint8_t maximum = 0;
			for (size_t index = 0; index < pixels.size(); index += 4)
			{
				maximum = std::max(maximum, pixels[index]);
			}
			return maximum;
		}
	}
	bool runRenderGraphGpuTests(RenderSystem* renderSystem, std::string* failure)
	{
		auto fail = [&](std::string const& message) { if (failure) *failure = message; return false; };
		if (!renderSystem) return fail("RenderSystem is null");
		std::string stage = "initial colour passes";
		try
		{
			GraphImageDesc colour;
			colour.format = GraphImageFormat::Rgba8;
			colour.usage = GraphImageUsage::ColourAttachment | GraphImageUsage::Sampled;
			RenderGraph graph;
			auto first = graph.createImage("GpuTestFirst", colour);
			auto absoluteColour=colour;absoluteColour.absoluteSize={17,19};auto second = graph.createImage("GpuTestSecond", absoluteColour);
			auto firstPass = graph.addPass("GpuTestClear", GraphPassType::Fullscreen);
			first = graph.writeColour(firstPass, first, GraphLoadOp::Clear, GraphStoreOp::Store, glm::vec4(1, 0, 0, 0.25f));
			auto secondPass = graph.addPass("GpuTestChain", GraphPassType::Fullscreen);
			graph.readSampled(secondPass, first);
			second = graph.writeColour(secondPass, second, GraphLoadOp::Clear, GraphStoreOp::Store, glm::vec4(0, 1, 0, 1));

			RenderGraphTargets targets(renderSystem);
			auto plan = graph.buildAllocationPlan({ 64, 48 });bool absolutePreserved=false;for(auto const& lifetime:plan.allocatedImages)if(lifetime.image.id==second.id&&lifetime.size==glm::uvec2(17,19))absolutePreserved=true;if(!absolutePreserved)return fail("absolute graph image dimensions followed the viewport");
			targets.allocate(plan);
			auto firstTarget = targets.get(first);
			if (!firstTarget || firstTarget->getWidth() != 64 || firstTarget->getHeight() != 48) return fail("initial graph target dimensions are wrong");
			std::weak_ptr<RenderTarget> releasedTarget = firstTarget;
			firstTarget.reset();
			RenderGraphExecutor executor(renderSystem);
			executor.setPassCallback(firstPass, [](RenderGraphExecutionContext const&) {});
			executor.setPassCallback(secondPass, [](RenderGraphExecutionContext const&) {});
			executor.execute(graph, targets, renderSystem->getCaps());
			if (!nearColour(readFirstPixel(targets.get(first)), { 255, 0, 0, 64 })) return fail("first pass clear colour/alpha readback failed");
			if (!nearColour(readFirstPixel(targets.get(second)), { 0, 255, 0, 255 })) return fail("second pass clear colour readback failed");

			auto resized = graph.buildAllocationPlan({ 37, 29 });
			targets.allocate(resized);
			auto resizedTarget = targets.get(first);
			if (!resizedTarget || resizedTarget->getWidth() != 37 || resizedTarget->getHeight() != 29) return fail("resized graph target dimensions are wrong");
			auto retainedTarget=resizedTarget;bool invalidPlanRejected=false;try{RenderGraphAllocationPlan invalidPlan;targets.allocate(invalidPlan);}catch(...){invalidPlanRejected=true;}if(!invalidPlanRejected||targets.get(first)!=retainedTarget)return fail("failed graph allocation did not retain the prior generation");
			resizedTarget.reset();retainedTarget.reset();

			stage="physical MSAA colour/depth allocation and resolve";
			for(uint32_t samples:{2u,4u,8u})if(renderSystem->getCaps().supportsMsaa(samples)){targets.allocatePhysical(plan,samples);auto write=dynamic_cast<RenderTexture*>(targets.getWriteTarget(first).get());auto resolved=dynamic_cast<RenderTexture*>(targets.get(first).get());if(!write||!resolved||write==resolved||write->getSamples()!=samples||resolved->getSamples()!=1)return fail("MSAA colour write/resolve targets are invalid");executor.execute(graph,targets,renderSystem->getCaps());if(!nearColour(readFirstPixel(targets.get(first)),{255,0,0,64}))return fail("MSAA colour/alpha resolve readback failed");}
			GraphImageDesc depthDesc;depthDesc.format=GraphImageFormat::Depth24;depthDesc.usage=GraphImageUsage::DepthAttachment|GraphImageUsage::Sampled;RenderGraph depthGraph;auto depthImage=depthGraph.createImage("GpuTestMsaaDepth",depthDesc);auto depthPass=depthGraph.addPass("GpuTestMsaaDepthClear",GraphPassType::Fullscreen);depthImage=depthGraph.writeDepth(depthPass,depthImage,GraphLoadOp::Clear,GraphStoreOp::Store,0.25f);RenderGraphExecutor depthExecutor(renderSystem);depthExecutor.setPassCallback(depthPass,[](RenderGraphExecutionContext const&){});auto depthPlan=depthGraph.buildAllocationPlan({32,24});for(uint32_t samples:{2u,4u,8u})if(renderSystem->getCaps().supportsMsaa(samples)){targets.allocatePhysical(depthPlan,samples);auto write=dynamic_cast<RenderTexture*>(targets.getWriteTarget(depthImage).get());auto resolved=dynamic_cast<RenderTexture*>(targets.get(depthImage).get());if(!write||!resolved||write->getSamples()!=samples||resolved->getSamples()!=1)return fail("MSAA depth write/resolve targets are invalid");depthExecutor.execute(depthGraph,targets,renderSystem->getCaps());auto value=readFirstDepth(targets.get(depthImage));if(value<0.24f||value>0.26f)return fail("MSAA depth resolve readback failed: "+std::to_string(value));}

			stage = "curated format allocation";
			std::vector<GraphImageFormat> const supportedFormats{
				GraphImageFormat::R8, GraphImageFormat::Rg8, GraphImageFormat::Rgba8, GraphImageFormat::Srgb8Alpha8,
				GraphImageFormat::R16f, GraphImageFormat::Rg16f, GraphImageFormat::Rgba16f,
				GraphImageFormat::R32f, GraphImageFormat::Rg32f, GraphImageFormat::Rgba32f,
				GraphImageFormat::R11g11b10f, GraphImageFormat::Rgb10a2,
				GraphImageFormat::Depth16, GraphImageFormat::Depth24, GraphImageFormat::Depth32f,
				GraphImageFormat::Depth24Stencil8, GraphImageFormat::Depth32fStencil8
			};
			for (size_t formatIndex = 0; formatIndex < supportedFormats.size(); ++formatIndex)
			{
				stage = "curated format allocation index " + std::to_string(formatIndex);
				auto const format = supportedFormats[formatIndex];
				bool const depthFormat = format >= GraphImageFormat::Depth16;
				GraphImageDesc formatDesc;
				formatDesc.format = format;
				formatDesc.usage = depthFormat ? GraphImageUsage::DepthAttachment : GraphImageUsage::ColourAttachment;
				if (format == GraphImageFormat::Srgb8Alpha8) formatDesc.colourSpace = TextureColourSpace::Srgb;
				RenderGraph formatGraph;
				auto image = formatGraph.createImage("GpuFormat" + std::to_string(formatIndex), formatDesc);
				auto pass = formatGraph.addPass("GpuFormatPass" + std::to_string(formatIndex), GraphPassType::Fullscreen);
				image = depthFormat ? formatGraph.writeDepth(pass, image, GraphLoadOp::Clear) : formatGraph.writeColour(pass, image, GraphLoadOp::Clear);
				RenderGraphTargets formatTargets(renderSystem);
				formatTargets.allocate(formatGraph.buildAllocationPlan({ 8, 8 }));
				RenderGraphExecutor formatExecutor(renderSystem);
				formatExecutor.setPassCallback(pass, [](RenderGraphExecutionContext const&) {});
				formatExecutor.execute(formatGraph, formatTargets, renderSystem->getCaps());
				if (!formatTargets.get(image)) return fail("curated graph format allocation failed");
			}
			stage = "depth diagnostic";
			GraphImageDesc diagnosticDepthDesc;
			diagnosticDepthDesc.format = GraphImageFormat::Depth32f;
			diagnosticDepthDesc.usage = GraphImageUsage::DepthAttachment;
			RenderGraph diagnosticDepthGraph;
			auto diagnosticDepthImage = diagnosticDepthGraph.createImage("GpuDiagnosticDepth", diagnosticDepthDesc);
			auto diagnosticDepthPass = diagnosticDepthGraph.addPass("GpuDiagnosticDepthPass", GraphPassType::Fullscreen);
			diagnosticDepthImage = diagnosticDepthGraph.writeDepth(diagnosticDepthPass, diagnosticDepthImage, GraphLoadOp::Clear, GraphStoreOp::Store, 0.0f);
			RenderGraphTargets diagnosticDepthTargets(renderSystem);
			diagnosticDepthTargets.allocate(diagnosticDepthGraph.buildAllocationPlan({ 8, 8 }));
			RenderGraphExecutor diagnosticDepthExecutor(renderSystem);
			diagnosticDepthExecutor.setPassCallback(diagnosticDepthPass, [](RenderGraphExecutionContext const&) {});
			diagnosticDepthExecutor.execute(diagnosticDepthGraph, diagnosticDepthTargets, renderSystem->getCaps());
			RenderTextureOptions diagnosticDepthOutputOptions;
			auto diagnosticDepthOutput = renderSystem->createRenderTexture("GpuDiagnosticDepthOutput", 8, 8, diagnosticDepthOutputOptions);
			RenderSystem::TextureDiagnosticOptions diagnosticDepthInspect;
			diagnosticDepthInspect.mode = RenderSystem::TextureDiagnosticMode::Depth;
			renderSystem->renderTextureDiagnostic(static_cast<RenderTexture*>(diagnosticDepthTargets.get(diagnosticDepthImage).get()), diagnosticDepthOutput, diagnosticDepthInspect);
			if (!nearColour(readFirstPixel(diagnosticDepthOutput), { 255, 255, 255, 255 })) return fail("depth diagnostic visualization failed");

			stage = "transient aliasing";
			RenderGraph aliasGraph;
			auto aliasFirst = aliasGraph.createImage("GpuTestAliasFirst", colour);
			auto aliasMiddle = aliasGraph.createImage("GpuTestAliasMiddle", colour);
			auto aliasLast = aliasGraph.createImage("GpuTestAliasLast", colour);
			auto aliasPass0 = aliasGraph.addPass("GpuTestAlias0", GraphPassType::Fullscreen);
			aliasFirst = aliasGraph.writeColour(aliasPass0, aliasFirst, GraphLoadOp::Clear, GraphStoreOp::Store, glm::vec4(1, 0, 0, 1));
			auto aliasPass1 = aliasGraph.addPass("GpuTestAlias1", GraphPassType::Fullscreen);
			aliasGraph.readSampled(aliasPass1, aliasFirst);
			aliasMiddle = aliasGraph.writeColour(aliasPass1, aliasMiddle, GraphLoadOp::Clear, GraphStoreOp::Store, glm::vec4(0, 1, 0, 1));
			auto aliasPass2 = aliasGraph.addPass("GpuTestAlias2", GraphPassType::Fullscreen);
			aliasGraph.readSampled(aliasPass2, aliasMiddle);
			aliasLast = aliasGraph.writeColour(aliasPass2, aliasLast, GraphLoadOp::Clear, GraphStoreOp::Store, glm::vec4(0, 0, 1, 1));
			auto aliasPlan = aliasGraph.buildAllocationPlan({ 24, 24 });
			if (!aliasPlan.valid || aliasPlan.allocatedImages.size() != 3 || aliasPlan.estimatedPhysicalBytes == 0)
				return fail("allocation introspection report is incomplete");
			if (aliasPlan.allocatedImages[0].physicalAllocation != aliasPlan.allocatedImages[2].physicalAllocation ||
				aliasPlan.allocatedImages[0].physicalAllocation == aliasPlan.allocatedImages[1].physicalAllocation)
				return fail("allocation introspection alias groups are incorrect");
			RenderGraphTargets aliasTargets(renderSystem);
			aliasTargets.allocate(aliasPlan);
			if (aliasTargets.get(aliasFirst) != aliasTargets.get(aliasLast)) return fail("non-overlapping transient lifetimes were not aliased");
			if (aliasTargets.get(aliasFirst) == aliasTargets.get(aliasMiddle)) return fail("overlapping transient lifetimes were aliased");
			GraphRasterState rasterState;
			rasterState.explicitState = true;
			rasterState.depthTest = false;
			rasterState.cullMode = GraphCullMode::None;
			rasterState.blend = true;
			rasterState.sourceColourBlend = GraphBlendFactor::SourceAlpha;
			rasterState.destinationColourBlend = GraphBlendFactor::OneMinusSourceAlpha;
			aliasGraph.setPassRasterState(aliasPass0, rasterState);
			bool observedRasterState = false;
			RenderGraphExecutor aliasExecutor(renderSystem);
			aliasExecutor.setPassCallback(aliasPass0, [&](RenderGraphExecutionContext const&) { observedRasterState = glIsEnabled(GL_BLEND) && !glIsEnabled(GL_DEPTH_TEST) && !glIsEnabled(GL_CULL_FACE); });
			aliasExecutor.setPassCallback(aliasPass1, [](RenderGraphExecutionContext const&) {});
			aliasExecutor.setPassCallback(aliasPass2, [](RenderGraphExecutionContext const&) {});
			aliasExecutor.execute(aliasGraph, aliasTargets, renderSystem->getCaps());
			if (!observedRasterState) return fail("explicit graph raster state was not applied");
			if (aliasExecutor.getLastExecutionStats().size() != 3) return fail("per-pass execution statistics were not recorded");
			if (!nearColour(readFirstPixel(aliasTargets.get(aliasLast)), { 0, 0, 255, 255 })) return fail("aliased transient final output readback failed");

			stage = "mip chain";
			GraphImageDesc mipColour = colour;
			mipColour.mipLevels = 3;
			mipColour.params.minFilter = GL_LINEAR_MIPMAP_LINEAR;
			RenderGraph mipGraph;
			auto mipImage = mipGraph.createImage("GpuTestMipChain", mipColour);
			auto mipViewOutput = mipGraph.createImage("GpuTestMipViewOutput", colour);
			auto mipPass = mipGraph.addPass("GpuTestMipWrite", GraphPassType::Fullscreen);
			mipImage = mipGraph.writeColour(mipPass, mipImage, GraphLoadOp::Clear, GraphStoreOp::Store, glm::vec4(1, 0, 1, 1));
			auto mipViewPass = mipGraph.addPass("GpuTestMipView", GraphPassType::Fullscreen);
			mipGraph.bindSampler(mipViewPass, "MIP_INPUT", mipImage, 2);
			mipViewOutput = mipGraph.writeColour(mipViewPass, mipViewOutput, GraphLoadOp::Clear, GraphStoreOp::Store, glm::vec4(0));
			RenderGraphTargets mipTargets(renderSystem);
			mipTargets.allocate(mipGraph.buildAllocationPlan({ 16, 8 }));
			RenderGraphExecutor mipExecutor(renderSystem);
			mipExecutor.setPassCallback(mipPass, [](RenderGraphExecutionContext const&) {});
			bool observedMipView = false;
			mipExecutor.setPassCallback(mipViewPass, [&](RenderGraphExecutionContext const& context)
			{
				auto texture = static_cast<RenderTexture*>(context.getImage(mipImage).get());
				GLint base = -1, maximum = -1;
				GL_CHECK(glBindTexture(GL_TEXTURE_2D, texture->getColourAttachmentId(0)));
				GL_CHECK(glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, &base));
				GL_CHECK(glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, &maximum));
				observedMipView = base == 2 && maximum == 2;
			});
			mipExecutor.execute(mipGraph, mipTargets, renderSystem->getCaps());
			if (!observedMipView) return fail("explicit sampled mip view was not applied");
			if (!nearColour(readFirstPixel(mipTargets.get(mipImage), 2), { 255, 0, 255, 255 })) return fail("generated mip level readback failed");
			RenderTextureOptions diagnosticOptions;
			auto diagnosticTarget = renderSystem->createRenderTexture("GpuTestDiagnostic", 8, 8, diagnosticOptions);
			RenderSystem::TextureDiagnosticOptions inspectOptions;
			inspectOptions.mode = RenderSystem::TextureDiagnosticMode::Green;
			inspectOptions.mipLevel = 2;
			renderSystem->renderTextureDiagnostic(static_cast<RenderTexture*>(mipTargets.get(mipImage).get()), diagnosticTarget, inspectOptions);
			auto diagnosticPixel = readFirstPixel(diagnosticTarget);
			if (!nearColour(diagnosticPixel, { 0, 0, 0, 255 })) return fail("graph image mip/channel diagnostic visualization failed (pixel=" + std::to_string(diagnosticPixel[0]) + "," + std::to_string(diagnosticPixel[1]) + "," + std::to_string(diagnosticPixel[2]) + "," + std::to_string(diagnosticPixel[3]) + ")");
			GL_CHECK(glFinish());
			mipExecutor.execute(mipGraph, mipTargets, renderSystem->getCaps());
			if (mipExecutor.getLastExecutionStats().empty() || !mipExecutor.getLastExecutionStats().front().gpuTimingAvailable) return fail("asynchronous per-pass GPU timing was not collected");

			stage = "explicit mip attachment";
			RenderGraph explicitMipGraph;
			auto explicitMip = explicitMipGraph.createImage("GpuTestExplicitMip", mipColour);
			auto explicitMipPass = explicitMipGraph.addPass("GpuTestExplicitMipWrite", GraphPassType::Fullscreen);
			explicitMip = explicitMipGraph.writeColour(explicitMipPass, explicitMip, GraphLoadOp::Clear, GraphStoreOp::Store, glm::vec4(0, 1, 1, 1), 2);
			RenderGraphTargets explicitMipTargets(renderSystem);
			explicitMipTargets.allocate(explicitMipGraph.buildAllocationPlan({ 16, 8 }));
			RenderGraphExecutor explicitMipExecutor(renderSystem);
			explicitMipExecutor.setPassCallback(explicitMipPass, [](RenderGraphExecutionContext const&) {});
			explicitMipExecutor.execute(explicitMipGraph, explicitMipTargets, renderSystem->getCaps());
			if (!nearColour(readFirstPixel(explicitMipTargets.get(explicitMip), 2), { 0, 255, 255, 255 })) return fail("explicit mip attachment readback failed");

			mipColour.mipLevels = 8;
			RenderGraph invalidMipGraph;
			auto invalidMip = invalidMipGraph.createImage("GpuTestInvalidMipChain", mipColour);
			auto invalidMipPass = invalidMipGraph.addPass("GpuTestInvalidMipWrite", GraphPassType::Fullscreen);
			invalidMipGraph.writeColour(invalidMipPass, invalidMip);
			if (invalidMipGraph.buildAllocationPlan({ 8, 8 }).valid) return fail("oversized mip chain was accepted");

			stage = "MRT readback";
			if (renderSystem->getCaps().maxDrawBuffers >= 2 && renderSystem->getCaps().maxColourAttachments >= 2)
			{
				RenderGraph mrt;
				auto left = mrt.createImage("GpuTestMrt0", colour);
				auto right = mrt.createImage("GpuTestMrt1", colour);
				auto pass = mrt.addPass("GpuTestMrt", GraphPassType::Scene);
				left = mrt.writeColour(pass, left, GraphLoadOp::Clear, GraphStoreOp::Store, glm::vec4(0, 0, 1, 1));
				right = mrt.writeColour(pass, right, GraphLoadOp::Clear, GraphStoreOp::Store, glm::vec4(1, 1, 0, 1));
				RenderGraphTargets mrtTargets(renderSystem);
				mrtTargets.allocate(mrt.buildAllocationPlan({ 32, 32 }));
				RenderGraphExecutor mrtExecutor(renderSystem);
				mrtExecutor.setPassCallback(pass, [](RenderGraphExecutionContext const&) {});
				mrtExecutor.execute(mrt, mrtTargets, renderSystem->getCaps());
				if (!nearColour(readFirstPixel(mrtTargets.get(left)), { 0, 0, 255, 255 })) return fail("MRT location 0 readback failed");
				if (!nearColour(readFirstPixel(mrtTargets.get(right)), { 255, 255, 0, 255 })) return fail("MRT location 1 readback failed");
			}

			stage = "screen-space text overlay";
			RenderTextureOptions textTargetOptions;
			auto textTarget = renderSystem->createRenderTexture(
				"GpuTestTextOverlay",
				renderSystem->getWindowWidth(),
				renderSystem->getWindowHeight(),
				textTargetOptions);
			renderSystem->setRenderTarget(textTarget);
			renderSystem->clearScreen(Colour::Black);
			renderSystem->renderText("RenderGraph text overlay", 8, 0, Colour::White);
			GL_CHECK(glFinish());
			bool textVisible = containsVisiblePixel(textTarget);
			if (!textVisible)
			{
				renderSystem->renderToScreen();
				return fail("screen-space text overlay produced no visible pixels");
			}

			renderSystem->clearScreen(Colour::Black);
			renderSystem->renderTextFormatted("[#FF0000FF]D", 8, 0);
			GL_CHECK(glFinish());
			uint8_t opaqueRed = maximumRed(textTarget);

			renderSystem->clearScreen(Colour::Black);
			renderSystem->renderTextFormatted("[#FF000080]D", 8, 0);
			GL_CHECK(glFinish());
			uint8_t translucentRed = maximumRed(textTarget);
			renderSystem->renderToScreen();
			if (opaqueRed < 32 || translucentRed < 8 || translucentRed >= opaqueRed * 3 / 4)
			{
				return fail("formatted text alpha was not applied to glyph coverage");
			}

			stage = "TAA jitter and camera-cut revisions";auto jitter0=taaHaltonJitter(0),jitter1=taaHaltonJitter(1),jitter7=taaHaltonJitter(7);if(glm::distance(jitter0,glm::vec2(0.0f,-1.0f/6.0f))>0.0001f||glm::distance(jitter1,glm::vec2(-0.25f,1.0f/6.0f))>0.0001f||glm::distance(jitter7,glm::vec2(-0.4375f,0.3888889f))>0.0001f||taaHaltonJitter(8)!=jitter0)return fail("eight-sample Halton jitter sequence is incorrect");Camera taaCamera({0,0,5},0,0,0,60,1);auto unjittered=taaCamera.getProjectionTransform();taaCamera.setProjectionJitter({0.01f,-0.02f});auto jittered=taaCamera.getProjectionTransform();if(jittered[2][0]==unjittered[2][0]||jittered[2][1]==unjittered[2][1])return fail("camera projection jitter was not applied");auto cutRevision=taaCamera.getCutRevision();taaCamera.markCut();if(taaCamera.getCutRevision()!=cutRevision+1)return fail("explicit camera-cut revision failed");taaCamera.setProjectionJitter({0,0});

			stage = "transactional named-output processor";
			RenderGraph outputGraph;GraphImageDesc outputDesc;outputDesc.format=GraphImageFormat::Rgba8;outputDesc.usage=GraphImageUsage::ColourAttachment|GraphImageUsage::Sampled;outputGraph.createImage("Presentation",outputDesc);RenderPipelineOutput output;output.name="Main";output.image="Presentation";output.antiAliasing.msaa=AntiAliasingSamples::X4;output.antiAliasing.ssaa=AntiAliasingSamples::X4;output.antiAliasing.taa=true;output.antiAliasing.fxaa=true;RenderOutputProcessor processor(renderSystem,"GpuTestOutput");processor.rebuild({output},outputGraph,{{"Main",textTarget}},{});auto generation=processor.getGeneration();if(generation==0||processor.getPlans().size()!=1||processor.getPlans().front().rasterSamples!=4||processor.getPlans().front().rasterSize!=glm::uvec2(textTarget->getWidth()*2,textTarget->getHeight()*2)||processor.getPlans().front().physicalImages.size()!=7)return fail("named-output physical plan/history ownership is incomplete");processor.rebuild({output},outputGraph,{{"Main",textTarget}},{});if(processor.getGeneration()!=generation)return fail("unchanged output plan replaced its generation");auto oldInput=processor.getInput("Main");bool rejected=false;try{auto invalid=output;invalid.image="Missing";processor.rebuild({invalid},outputGraph,{{"Main",textTarget}},{});}catch(...){rejected=true;}if(!rejected||processor.getGeneration()!=generation||processor.getInput("Main")!=oldInput)return fail("failed output generation did not retain prior resources");renderSystem->setRenderTarget(oldInput);renderSystem->clearScreen(Colour(1,0,0,0.25f));RenderTextureOptions taaDepthOptions;taaDepthOptions.numAttachments=0;taaDepthOptions.depthAttachment=RenderTextureDepthAttachment::DepthTexture;auto taaDepth=renderSystem->createRenderTexture("GpuTestTaaDepth",oldInput->getWidth(),oldInput->getHeight(),taaDepthOptions);renderSystem->setRenderTarget(taaDepth);float taaDepthValue=0.5f;GL_CHECK(glClearBufferfv(GL_DEPTH,0,&taaDepthValue));TaaFrameContext taaFrame;taaFrame.frameSerial=1;taaFrame.resetHistory=true;processor.present("Main",textTarget,{},taaDepth,&taaFrame);GL_CHECK(glFinish());if(!nearColour(readFirstPixel(textTarget),{255,0,0,64})||!processor.hasValidTaaHistory("Main")||processor.getTaaHistoryResetCount("Main")!=1)return fail("TAA first-frame history/alpha initialization failed");taaFrame.frameSerial=2;taaFrame.resetHistory=false;processor.present("Main",textTarget,{},taaDepth,&taaFrame);if(processor.getTaaHistoryResetCount("Main")!=1)return fail("TAA valid consecutive frame reset history");taaFrame.frameSerial=4;processor.present("Main",textTarget,{},taaDepth,&taaFrame);if(processor.getTaaHistoryResetCount("Main")!=2)return fail("TAA skipped-frame reset failed");taaFrame.frameSerial=5;taaFrame.resetHistory=true;processor.present("Main",textTarget,{},taaDepth,&taaFrame);if(processor.getTaaHistoryResetCount("Main")!=3)return fail("TAA explicit camera-cut reset failed");auto resizedTaaTarget=renderSystem->createRenderTexture("GpuTestTaaResize",31,23,RenderTextureOptions{});processor.rebuild({output},outputGraph,{{"Main",resizedTaaTarget}},{});if(processor.hasValidTaaHistory("Main"))return fail("TAA resize/generation replacement retained history");output.antiAliasing.taa=false;auto smallSsaaTarget=renderSystem->createRenderTexture("GpuTestSsaaOutput",31,23,RenderTextureOptions{});for(auto factor:{AntiAliasingSamples::X2,AntiAliasingSamples::X4,AntiAliasingSamples::X8}){output.antiAliasing.ssaa=factor;processor.rebuild({output},outputGraph,{{"Main",smallSsaaTarget}},{});auto const& ssaaPlan=processor.getPlans().front();if(ssaaPlan.rasterSize!=glm::uvec2(ssaaDimension(31,factor),ssaaDimension(23,factor)))return fail("SSAA physical dimensions are incorrect");renderSystem->setRenderTarget(processor.getInput("Main"));renderSystem->clearScreen(Colour(0.2f,0.4f,0.8f,0.375f));processor.present("Main",smallSsaaTarget);GL_CHECK(glFinish());if(!nearColour(readFirstPixel(smallSsaaTarget),{51,102,204,96}))return fail("SSAA factor Lanczos readback did not preserve colour/alpha");if(factor==AntiAliasingSamples::X4){auto input=dynamic_cast<RenderTexture*>(processor.getInput("Main").get());std::vector<uint8_t> checker(input->getWidth()*input->getHeight()*4);for(size_t y=0;y<input->getHeight();++y)for(size_t x=0;x<input->getWidth();++x){auto value=(uint8_t)(((x+y)&1)?255:0);auto index=(y*input->getWidth()+x)*4;checker[index]=checker[index+1]=checker[index+2]=value;checker[index+3]=255;}GL_CHECK(glBindTexture(GL_TEXTURE_2D,input->getColourAttachmentId(0)));GL_CHECK(glTexSubImage2D(GL_TEXTURE_2D,0,0,0,(GLsizei)input->getWidth(),(GLsizei)input->getHeight(),GL_RGBA,GL_UNSIGNED_BYTE,checker.data()));GL_CHECK(glBindTexture(GL_TEXTURE_2D,0));processor.present("Main",smallSsaaTarget);GL_CHECK(glFinish());auto filtered=readPixels(smallSsaaTarget);auto centre=((smallSsaaTarget->getHeight()/2)*smallSsaaTarget->getWidth()+smallSsaaTarget->getWidth()/2)*4;if(filtered[centre]<100||filtered[centre]>155||filtered[centre+3]<254)return fail("Lanczos checkerboard downsample did not filter/preserve alpha");}}

			stage="TAA accumulation, neighbourhood clamp, and depth rejection";output.antiAliasing.ssaa=AntiAliasingSamples::Off;output.antiAliasing.taa=true;output.antiAliasing.fxaa=false;auto taaOutput=renderSystem->createRenderTexture("GpuTestTaaOutput",9,9,RenderTextureOptions{});processor.rebuild({output},outputGraph,{{"Main",taaOutput}},{});auto taaInputTarget=processor.getInput("Main");auto taaInput=dynamic_cast<RenderTexture*>(taaInputTarget.get());RenderTextureOptions smallDepthOptions;smallDepthOptions.numAttachments=0;smallDepthOptions.depthAttachment=RenderTextureDepthAttachment::DepthTexture;auto currentDepth=renderSystem->createRenderTexture("GpuTestTaaCurrentDepth",9,9,smallDepthOptions);renderSystem->setRenderTarget(taaInputTarget);renderSystem->clearScreen(Colour(0.5f,0.5f,0.5f,0.5f));renderSystem->setRenderTarget(currentDepth);float halfDepth=0.5f;GL_CHECK(glClearBufferfv(GL_DEPTH,0,&halfDepth));TaaFrameContext accumulationFrame;accumulationFrame.frameSerial=10;accumulationFrame.resetHistory=true;processor.present("Main",taaOutput,{},currentDepth,&accumulationFrame);std::vector<uint8_t> taaChecker(9*9*4);for(size_t pixel=0;pixel<81;++pixel){auto value=(uint8_t)((((pixel%9)+(pixel/9))&1)?255:0);taaChecker[pixel*4]=taaChecker[pixel*4+1]=taaChecker[pixel*4+2]=value;taaChecker[pixel*4+3]=128;}GL_CHECK(glBindTexture(GL_TEXTURE_2D,taaInput->getColourAttachmentId(0)));GL_CHECK(glTexSubImage2D(GL_TEXTURE_2D,0,0,0,9,9,GL_RGBA,GL_UNSIGNED_BYTE,taaChecker.data()));GL_CHECK(glBindTexture(GL_TEXTURE_2D,0));accumulationFrame.frameSerial=11;accumulationFrame.resetHistory=false;processor.present("Main",taaOutput,{},currentDepth,&accumulationFrame);GL_CHECK(glFinish());auto accumulated=readPixels(taaOutput);auto accumulatedCentre=(4*9+4)*4;if(accumulated[accumulatedCentre]<100||accumulated[accumulatedCentre]>130||accumulated[accumulatedCentre+3]<126||accumulated[accumulatedCentre+3]>130)return fail("TAA static-history blend/neighbourhood clamp failed");renderSystem->setRenderTarget(taaInputTarget);renderSystem->clearScreen(Colour(0,0,1,0.25f));renderSystem->setRenderTarget(currentDepth);float changedDepth=0.8f;GL_CHECK(glClearBufferfv(GL_DEPTH,0,&changedDepth));accumulationFrame.frameSerial=12;processor.present("Main",taaOutput,{},currentDepth,&accumulationFrame);GL_CHECK(glFinish());if(!nearColour(readFirstPixel(taaOutput),{0,0,255,64}))return fail("TAA depth-inconsistent history was not rejected");

			stage="fixed high-quality FXAA LDR processing";output.antiAliasing.taa=false;output.antiAliasing.fxaa=true;auto fxaaOutput=renderSystem->createRenderTexture("GpuTestFxaaOutput",16,16,RenderTextureOptions{});processor.rebuild({output},outputGraph,{{"Main",fxaaOutput}},{});auto fxaaInput=dynamic_cast<RenderTexture*>(processor.getInput("Main").get());std::vector<uint8_t> staircase(16*16*4);for(size_t y=0;y<16;++y)for(size_t x=0;x<16;++x){auto index=(y*16+x)*4;auto value=(uint8_t)(x>y?255:0);staircase[index]=staircase[index+1]=staircase[index+2]=value;staircase[index+3]=96;}GL_CHECK(glBindTexture(GL_TEXTURE_2D,fxaaInput->getColourAttachmentId(0)));GL_CHECK(glTexSubImage2D(GL_TEXTURE_2D,0,0,0,16,16,GL_RGBA,GL_UNSIGNED_BYTE,staircase.data()));GL_CHECK(glBindTexture(GL_TEXTURE_2D,0));processor.present("Main",fxaaOutput);GL_CHECK(glFinish());auto antialiased=readPixels(fxaaOutput);size_t softened=0;for(size_t pixel=0;pixel<256;++pixel){auto red=antialiased[pixel*4];if(red>8&&red<247)++softened;if(std::abs((int)antialiased[pixel*4+3]-96)>1)return fail("FXAA did not preserve alpha");}if(!softened)return fail("FXAA did not soften a staircase edge");

			targets.clear();
			if (!releasedTarget.expired()) return fail("cleared graph target remains referenced");
			renderSystem->renderToScreen();
			return true;
		}
		catch (MppException const& exception)
		{
			renderSystem->renderToScreen();
			return fail(stage + ": " + exception.what() + " at " + exception.getFile() + ":" + std::to_string(exception.getLine()));
		}
		catch (std::exception const& exception)
		{
			renderSystem->renderToScreen();
			return fail(stage + ": " + exception.what());
		}
	}
}
