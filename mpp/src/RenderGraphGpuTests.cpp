#include <memory>

#include "mpp/RenderGraphGpuTests.h"
#include "mpp/RenderGraph.h"
#include "mpp/RenderGraphExecutor.h"
#include "mpp/RenderGraphTargets.h"
#include "mpp/RenderSystem.h"

namespace mpp
{
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

			auto resized = graph.buildAllocationPlan({ 37, 29 });
			targets.allocate(resized);
			auto resizedTarget = targets.get(first);
			if (!resizedTarget || resizedTarget->getWidth() != 37 || resizedTarget->getHeight() != 29) return fail("resized graph target dimensions are wrong");
			resizedTarget.reset();

			if (renderSystem->getCaps().maxDrawBuffers >= 2 && renderSystem->getCaps().maxColourAttachments >= 2)
			{
				RenderGraph mrt;
				auto left = mrt.createImage("GpuTestMrt0", colour);
				auto right = mrt.createImage("GpuTestMrt1", colour);
				auto pass = mrt.addPass("GpuTestMrt", GraphPassType::Scene);
				mrt.writeColour(pass, left, GraphLoadOp::Clear, GraphStoreOp::Store, glm::vec4(0));
				mrt.writeColour(pass, right, GraphLoadOp::Clear, GraphStoreOp::Store, glm::vec4(0));
				RenderGraphTargets mrtTargets(renderSystem);
				mrtTargets.allocate(mrt.buildAllocationPlan({ 32, 32 }));
				RenderGraphExecutor mrtExecutor(renderSystem);
				mrtExecutor.setPassCallback(pass, [](RenderGraphExecutionContext const&) {});
				mrtExecutor.execute(mrt, mrtTargets, renderSystem->getCaps());
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
