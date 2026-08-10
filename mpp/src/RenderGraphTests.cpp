#include "mpp/RenderGraph.h"
#include "mpp/RenderGraphBuiltInPasses.h"
#include "mpp/RenderGraphPassFactoryRegistry.h"
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
		if (!result.valid || result.passOrder.size() != 2) return fail("valid two-pass graph was rejected");if(valid.getImageVersionCount(scene.id)!=2)return fail("render graph image version inventory is incorrect");
		auto editedDesc=valid.getImageInfo({0,0}).desc;editedDesc.mipLevels=2;valid.setImageDesc({0,0},editedDesc);if(valid.getImageInfo({0,0}).desc.mipLevels!=2)return fail("graph image descriptor edit was not retained");
		RenderGraph copied(valid);copied.setPassEnabled(scenePass,false);if(valid.getPassInfo(scenePass).enabled==copied.getPassInfo(scenePass).enabled||copied.getPassCount()!=valid.getPassCount())return fail("deep RenderGraph copy is not independent");RenderGraph assigned;assigned=valid;if(assigned.getPassCount()!=valid.getPassCount()||!assigned.compile().valid)return fail("RenderGraph copy assignment lost topology");
		RenderGraph structural(valid);structural.setPassName({0},"SceneRenamed");structural.setImageName({0,0},"SceneTarget");auto duplicate=structural.duplicatePass({1},"BloomCopy");if(structural.getPassCount()!=3||structural.getPassInfo(duplicate).colourOutputs.empty())return fail("render graph pass duplication failed");structural.movePass(duplicate,1);if(structural.getPassInfo({1}).name!="BloomCopy")return fail("render graph pass move failed");structural.removePass({1});if(structural.getPassCount()!=2||structural.getImageVersionCount(1)!=2)return fail("render graph pass removal did not clean produced values");structural.removeImage({1,0});if(structural.getImageCount()!=1||!structural.getPassInfo({1}).colourOutputs.empty())return fail("render graph image removal did not clean pass references");
		RenderGraph attachments;auto attachmentA=attachments.createImage("A",colour),attachmentB=attachments.createImage("B",colour),attachmentOut=attachments.createImage("Out",colour);auto attachmentWriter=attachments.addPass("Writer");attachmentA=attachments.writeColour(attachmentWriter,attachmentA);attachments.setValueId(attachmentA,"Stable.Attachment");auto attachmentReader=attachments.addPass("Reader");attachments.bindSampler(attachmentReader,"TEX",attachmentA);attachments.writeColour(attachmentReader,attachmentOut);auto replacement=attachments.retargetColourOutput(attachmentWriter,0,attachmentB);if(attachments.getValueId(replacement)!="Stable.Attachment"||attachments.getPassInfo(attachmentReader).samplerBindings[0].image.id!=attachmentB.id||!attachments.compile().valid)return fail("attachment retargeting did not preserve stable dependent references");attachments.removeColourOutput(attachmentWriter,0);if(!attachments.getPassInfo(attachmentReader).sampledInputs.empty())return fail("attachment removal did not clean sampled references");

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
		RenderGraph reordered(outOfOrder);reordered.reorderPasses(dependencyOrder.passOrder);if(!reordered.compile().valid||reordered.getPassInfo({0}).name!="Producer")return fail("explicit dependency pass reorder failed");

		outOfOrder.setPassEnabled(producer, false);
		if (outOfOrder.compile().valid) return fail("value produced by a disabled pass was accepted");
		if (outOfOrder.getPassInfo(producer).enabled) return fail("disabled pass state was not retained");

		RenderGraphPassFactoryRegistry registry;
		registerBuiltInRenderGraphPasses(registry);
		if (!registry.findMetadata("MPP.PbrScene") || !registry.findMetadata("MPP.CustomFullscreen"))
			return fail("built-in pass authoring metadata was not registered");
		RenderGraph metadataGraph;
		auto metadataInput = metadataGraph.createImage("MetadataInput", colour);
		auto metadataProducer = metadataGraph.addPass("MetadataProducer");
		metadataInput = metadataGraph.writeColour(metadataProducer, metadataInput);
		metadataGraph.setPassEnabled(metadataProducer, false);
		auto metadataOutput = metadataGraph.createImage("MetadataOutput", colour);
		auto metadataPass = metadataGraph.addPass("MetadataBloom", GraphPassType::Fullscreen);
		metadataGraph.setPassCallbackFactory(metadataPass, "MPP.BloomExtract");
		metadataGraph.bindSampler(metadataPass, "TEX1", metadataInput);
		metadataGraph.writeColour(metadataPass, metadataOutput);
		if (registry.validate(metadataGraph).hasErrors()) return fail("valid pass authoring metadata contract was rejected");
		metadataGraph.setPassCallbackFactory(metadataPass, "Unknown.Factory");
		if (!registry.validate(metadataGraph).hasErrors()) return fail("unknown pass factory metadata was accepted");

		// A non-transient image holds contents that outlive the frame, so nothing may
		// be planned on top of it. The plan used to test only the incoming image for
		// transience and not the allocation it was joining, so a transient image with
		// a compatible descriptor and a disjoint lifetime landed on one. The real
		// allocator refuses this, so the damage was confined to the plan -- but
		// PipelineEditor reports physicalAllocation and estimatedPhysicalBytes
		// straight from it, and any future consumer inherits a corruption bug.
		GraphImageDesc persistent = colour; persistent.transient = false;
		RenderGraph aliasing;
		auto keep = aliasing.createImage("Keep", persistent);
		auto scratchA = aliasing.createImage("ScratchA", colour);
		auto scratchB = aliasing.createImage("ScratchB", colour);
		auto scratchC = aliasing.createImage("ScratchC", colour);
		auto aliasStep0 = aliasing.addPass("Alias0", GraphPassType::Fullscreen);
		keep = aliasing.writeColour(aliasStep0, keep);
		auto aliasStep1 = aliasing.addPass("Alias1", GraphPassType::Fullscreen);
		aliasing.readSampled(aliasStep1, keep); scratchA = aliasing.writeColour(aliasStep1, scratchA);
		auto aliasStep2 = aliasing.addPass("Alias2", GraphPassType::Fullscreen);
		aliasing.readSampled(aliasStep2, scratchA); scratchB = aliasing.writeColour(aliasStep2, scratchB);
		auto aliasStep3 = aliasing.addPass("Alias3", GraphPassType::Fullscreen);
		aliasing.readSampled(aliasStep3, scratchB); aliasing.writeColour(aliasStep3, scratchC);
		auto aliasingPlan = aliasing.buildAllocationPlan({ 32, 32 });
		if (!aliasingPlan.valid || aliasingPlan.allocatedImages.size() != 4) return fail("transient aliasing plan did not compile");
		auto allocationOf = [&](GraphImageHandle const& image)
		{
			for (auto const& lifetime : aliasingPlan.allocatedImages) if (lifetime.image.id == image.id) return lifetime.physicalAllocation;
			return UINT32_MAX;
		};
		// Keep lives over passes 0..1 and ScratchB over 2..3, so their lifetimes are
		// disjoint and only transience keeps them apart.
		if (allocationOf(keep) == allocationOf(scratchB))
			return fail("a transient graph image was planned on top of a non-transient allocation");
		// ScratchA (1..2) and ScratchC (3..3) are disjoint and both transient, so the
		// fix must not have simply stopped aliasing altogether.
		if (allocationOf(scratchA) != allocationOf(scratchC))
			return fail("disjoint transient graph images were not aliased onto one allocation");
		uint64_t distinctBytes = 0;
		for (auto const& lifetime : aliasingPlan.allocatedImages) if (lifetime.image.id != scratchC.id) distinctBytes += lifetime.estimatedBytes;
		if (aliasingPlan.estimatedPhysicalBytes != distinctBytes)
			return fail("planned physical byte estimate does not match its own allocation groups");

		return true;
	}
}
