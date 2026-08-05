#include <glew/glew.h>

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include "mpp/RenderGraphGpuTests.h"
#include "mpp/RenderGraph.h"
#include "mpp/RenderGraphExecutor.h"
#include "mpp/RenderGraphTargets.h"
#include "mpp/RenderSystem.h"
#include "mpp/RenderTexture.h"
#include "mpp/GLErrorCheck.h"

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

		bool nearColour(std::array<uint8_t, 4> const& pixel, std::array<uint8_t, 4> const& expected)
		{
			for (size_t i = 0; i < 4; ++i)
				if (std::abs((int)pixel[i] - (int)expected[i]) > 1) return false;
			return true;
		}
	}
	bool runRenderGraphGpuTests(RenderSystem* renderSystem, std::string* failure)
	{
		auto fail = [&](std::string const& message) { if (failure) *failure = message; return false; };
		if (!renderSystem) return fail("RenderSystem is null");
		try
		{
			GraphImageDesc colour;
			colour.format = GraphImageFormat::Rgba8;
			colour.usage = GraphImageUsage::ColourAttachment | GraphImageUsage::Sampled;
			RenderGraph graph;
			auto first = graph.createImage("GpuTestFirst", colour);
			auto second = graph.createImage("GpuTestSecond", colour);
			auto firstPass = graph.addPass("GpuTestClear", GraphPassType::Fullscreen);
			first = graph.writeColour(firstPass, first, GraphLoadOp::Clear, GraphStoreOp::Store, glm::vec4(1, 0, 0, 1));
			auto secondPass = graph.addPass("GpuTestChain", GraphPassType::Fullscreen);
			graph.readSampled(secondPass, first);
			second = graph.writeColour(secondPass, second, GraphLoadOp::Clear, GraphStoreOp::Store, glm::vec4(0, 1, 0, 1));

			RenderGraphTargets targets(renderSystem);
			auto plan = graph.buildAllocationPlan({ 64, 48 });
			targets.allocate(plan);
			auto firstTarget = targets.get(first);
			if (!firstTarget || firstTarget->getWidth() != 64 || firstTarget->getHeight() != 48) return fail("initial graph target dimensions are wrong");
			std::weak_ptr<RenderTarget> releasedTarget = firstTarget;
			firstTarget.reset();
			RenderGraphExecutor executor(renderSystem);
			executor.setPassCallback(firstPass, [](RenderGraphExecutionContext const&) {});
			executor.setPassCallback(secondPass, [](RenderGraphExecutionContext const&) {});
			executor.execute(graph, targets, renderSystem->getCaps());
			if (!nearColour(readFirstPixel(targets.get(first)), { 255, 0, 0, 255 })) return fail("first pass clear colour readback failed");
			if (!nearColour(readFirstPixel(targets.get(second)), { 0, 255, 0, 255 })) return fail("second pass clear colour readback failed");

			auto resized = graph.buildAllocationPlan({ 37, 29 });
			targets.allocate(resized);
			auto resizedTarget = targets.get(first);
			if (!resizedTarget || resizedTarget->getWidth() != 37 || resizedTarget->getHeight() != 29) return fail("resized graph target dimensions are wrong");
			resizedTarget.reset();

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
			RenderGraphTargets aliasTargets(renderSystem);
			aliasTargets.allocate(aliasGraph.buildAllocationPlan({ 24, 24 }));
			if (aliasTargets.get(aliasFirst) != aliasTargets.get(aliasLast)) return fail("non-overlapping transient lifetimes were not aliased");
			if (aliasTargets.get(aliasFirst) == aliasTargets.get(aliasMiddle)) return fail("overlapping transient lifetimes were aliased");
			RenderGraphExecutor aliasExecutor(renderSystem);
			aliasExecutor.setPassCallback(aliasPass0, [](RenderGraphExecutionContext const&) {});
			aliasExecutor.setPassCallback(aliasPass1, [](RenderGraphExecutionContext const&) {});
			aliasExecutor.setPassCallback(aliasPass2, [](RenderGraphExecutionContext const&) {});
			aliasExecutor.execute(aliasGraph, aliasTargets, renderSystem->getCaps());
			if (!nearColour(readFirstPixel(aliasTargets.get(aliasLast)), { 0, 0, 255, 255 })) return fail("aliased transient final output readback failed");

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

			targets.clear();
			if (!releasedTarget.expired()) return fail("cleared graph target remains referenced");
			renderSystem->renderToScreen();
			return true;
		}
		catch (std::exception const& exception)
		{
			renderSystem->renderToScreen();
			return fail(exception.what());
		}
	}
}
