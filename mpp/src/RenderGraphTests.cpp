#include "mpp/RenderGraph.h"
#include "mpp/RenderGraphTests.h"

namespace mpp
{
	bool runRenderGraphTopologyTests(std::string* failure)
	{
		auto fail = [&](std::string const& message) { if (failure) *failure = message; return false; };
		GraphImageDesc colour;
		colour.format = GraphImageFormat::Rgba16f;
		colour.usage = GraphImageUsage::ColourAttachment | GraphImageUsage::Sampled;

		RenderGraph valid;
		auto scene = valid.createImage("Scene", colour);
		auto bloom = valid.createImage("Bloom", colour);
		auto scenePass = valid.addPass("Scene", GraphPassType::Scene);
		scene = valid.writeColour(scenePass, scene, GraphLoadOp::Clear);
		auto bloomPass = valid.addPass("Bloom", GraphPassType::Fullscreen);
		valid.readSampled(bloomPass, scene);
		valid.writeColour(bloomPass, bloom);
		auto result = valid.compile();
		if (!result.valid || result.passOrder.size() != 2) return fail("valid two-pass graph was rejected");

		RenderGraph missingProducer;
		auto unwritten = missingProducer.createImage("Unwritten", colour);
		auto reader = missingProducer.addPass("Reader");
		missingProducer.readSampled(reader, unwritten);
		if (missingProducer.compile().valid) return fail("unwritten sampled image was accepted");

		RenderGraph feedback;
		auto feedbackImage = feedback.createImage("Feedback", colour);
		auto feedbackPass = feedback.addPass("FeedbackPass");
		auto written = feedback.writeColour(feedbackPass, feedbackImage);
		feedback.readSampled(feedbackPass, written);
		if (feedback.compile().valid) return fail("same-pass image feedback was accepted");

		valid.setValueId(scene, "Scene.AfterOpaque");
		if (valid.getValueId(scene) != "Scene.AfterOpaque" || valid.findValue("Scene.AfterOpaque").version != scene.version)
			return fail("stable graph value ID did not round-trip");

		RenderGraph outOfOrder;
		auto orderedImage = outOfOrder.createImage("Ordered", colour);
		auto orderedOutput = outOfOrder.createImage("OrderedOutput", colour);
		auto consumer = outOfOrder.addPass("Consumer", GraphPassType::Fullscreen);
		auto producer = outOfOrder.addPass("Producer", GraphPassType::Scene);
		orderedImage = outOfOrder.writeColour(producer, orderedImage);
		outOfOrder.setValueId(orderedImage, "Ordered.Produced");
		outOfOrder.readSampled(consumer, orderedImage);
		outOfOrder.writeColour(consumer, orderedOutput);
		if (outOfOrder.compile().valid) return fail("authored pass order ignored a later producer");
		auto dependencyOrder = outOfOrder.buildDependencyOrder();
		if (!dependencyOrder.valid || dependencyOrder.passOrder.size() != 2 ||
			dependencyOrder.passOrder[0].id != producer.id || dependencyOrder.passOrder[1].id != consumer.id)
			return fail("stable dependency auto-order is incorrect");

		outOfOrder.setPassEnabled(producer, false);
		if (outOfOrder.compile().valid) return fail("value produced by a disabled pass was accepted");
		if (outOfOrder.getPassInfo(producer).enabled) return fail("disabled pass state was not retained");

		return true;
	}
}
