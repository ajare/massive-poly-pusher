#include "mpp/Caps.h"
#include "mpp/RenderGraph.h"
#include "mpp/RenderGraphBuiltInPasses.h"
#include "mpp/RenderGraphPassFactoryRegistry.h"
#include "mpp/RenderGraphTests.h"
#include "mpp/ModelRenderParams.h"
#include "mpp/mesh/MeshSpecification.h"

namespace mpp
{
	bool runRenderGraphTopologyTests(std::string* failure)
	{
		auto fail = [&](std::string const& message) { if (failure) *failure = message; return false; };

		ModelRenderParams modelParams;
		auto const initialProgramSetRevision = modelParams.getProgramSetRevision();
		modelParams.setModelInstanceCount(2);
		modelParams.setMeshUniforms("Mesh", {});
		if (modelParams.getProgramSetRevision() != initialProgramSetRevision) return fail("non-program model parameters invalidated the visible program set");
		modelParams.setModelFlags(ModelRenderParams::Flag_Visible);
		auto const modelFlagsRevision = modelParams.getProgramSetRevision();
		modelParams.setMeshFlags("Mesh", 0);
		if (modelFlagsRevision <= initialProgramSetRevision || modelParams.getProgramSetRevision() <= modelFlagsRevision)
			return fail("visibility/material model parameters did not invalidate the visible program set");
		auto const unchangedFlagsRevision = modelParams.getProgramSetRevision();
		modelParams.setMeshFlags("Mesh", 0);
		if (modelParams.getProgramSetRevision() != unchangedFlagsRevision) return fail("unchanged visibility invalidated the visible program set");

		// Mesh layout identity feeds program caching. Every equality field must be
		// represented by an unambiguous canonical key, while the compact hash and
		// generated descriptor must distinguish common formerly-colliding layouts.
		auto makeMeshSpecification = [](mesh::Vertex::DataType type, bool normalised = false, size_t boundary = 0, std::string identifier = "POSITION")
		{
			mesh::MeshSpecification specification;
			auto* layout = specification.createVertexBufferAttributeLayout(false);
			layout->createAttribute(mesh::Vertex::Component::Position2, identifier, type, normalised, boundary);
			return specification;
		};
		auto floatLayout = makeMeshSpecification(mesh::Vertex::DataType::Float);
		auto identicalFloatLayout = floatLayout;
		auto integerLayout = makeMeshSpecification(mesh::Vertex::DataType::Int);
		auto normalisedLayout = makeMeshSpecification(mesh::Vertex::DataType::Float, true);
		auto paddedLayout = makeMeshSpecification(mesh::Vertex::DataType::Float, false, 16);
		auto identifiedLayout = makeMeshSpecification(mesh::Vertex::DataType::Float, false, 0, "CUSTOM_POSITION");
		auto offsetLayout = floatLayout; offsetLayout.getVertexBufferAttributeLayout(0).getAttribute(0).offsetInBytes = 4;
		if (floatLayout != identicalFloatLayout || floatLayout.getHashString() != identicalFloatLayout.getHashString() || floatLayout.getHashCode() != identicalFloatLayout.getHashCode())
			return fail("equal mesh layouts do not have stable canonical identities");
		for (auto const* different : { &integerLayout, &normalisedLayout, &paddedLayout, &identifiedLayout, &offsetLayout })
		{
			if (floatLayout == *different || floatLayout.getHashString() == different->getHashString() || floatLayout.getHashCode() == different->getHashCode() || floatLayout.getDescriptor("mesh_") == different->getDescriptor("mesh_"))
				return fail("mesh identity collapsed a differing attribute type, normalization, offset, padding, or identifier");
		}
		auto indexedLayout = floatLayout; indexedLayout.setIndexedVertices(true);
		auto dynamicLayout = floatLayout; dynamicLayout.setStorageType(mesh::VertexBufferStorageType::Dynamic);
		auto lineLayout = floatLayout; lineLayout.setPrimitiveType(mesh::Primitive::Type::Lines);
		auto staticLayout = mesh::MeshSpecification();
		staticLayout.createVertexBufferAttributeLayout(true)->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
		for (auto const* different : { &indexedLayout, &dynamicLayout, &lineLayout, &staticLayout })
			if (floatLayout.getHashString() == different->getHashString()) return fail("mesh identity omitted primitive, storage, indexing, or buffer-static state");
		mesh::MeshSpecification groupedLayout;
		auto* groupedFirst = groupedLayout.createVertexBufferAttributeLayout(false);
		groupedFirst->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
		groupedFirst->createAttribute(mesh::Vertex::Component::UserDefined2, "USER_A", mesh::Vertex::DataType::Float, false);
		mesh::MeshSpecification splitLayout;
		auto* splitFirst = splitLayout.createVertexBufferAttributeLayout(false);
		splitFirst->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
		auto* splitSecond = splitLayout.createVertexBufferAttributeLayout(false);
		splitSecond->createAttribute(mesh::Vertex::Component::UserDefined2, "USER_A", mesh::Vertex::DataType::Float, false);
		if (groupedLayout.getHashString() == splitLayout.getHashString()) return fail("mesh identity omitted vertex-buffer layout grouping");

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
		auto cacheAfterFirstCompile = valid.getPlanCacheStats();
		if (cacheAfterFirstCompile.compileMisses != 1) return fail("first graph compilation was not recorded as a cache miss");
		if (!valid.compile().valid || valid.getPlanCacheStats().compileHits <= cacheAfterFirstCompile.compileHits) return fail("unchanged graph compilation did not hit the plan cache");
		auto firstAllocation = valid.buildAllocationPlan({ 320, 180 });
		auto cacheAfterFirstAllocation = valid.getPlanCacheStats();
		auto repeatedAllocation = valid.buildAllocationPlan({ 320, 180 });
		if (!firstAllocation.valid || repeatedAllocation.allocatedImages.size() != firstAllocation.allocatedImages.size() || valid.getPlanCacheStats().allocationHits <= cacheAfterFirstAllocation.allocationHits)
			return fail("unchanged graph allocation did not hit the viewport plan cache");
		auto cacheBeforeEdit = valid.getPlanCacheStats();
		auto editedDesc=valid.getImageInfo({0,0}).desc;editedDesc.mipLevels=2;valid.setImageDesc({0,0},editedDesc);if(valid.getImageInfo({0,0}).desc.mipLevels!=2)return fail("graph image descriptor edit was not retained");
		if (!valid.compile().valid || valid.getPlanCacheStats().compileMisses <= cacheBeforeEdit.compileMisses || valid.getPlanCacheStats().invalidations <= cacheBeforeEdit.invalidations)
			return fail("graph edit did not invalidate the cached compilation");
		auto rebuiltAllocation = valid.buildAllocationPlan({ 320, 180 });
		if (!rebuiltAllocation.valid || rebuiltAllocation.allocatedImages.empty() || rebuiltAllocation.allocatedImages.front().desc.mipLevels != 2 || valid.getPlanCacheStats().allocationMisses <= cacheBeforeEdit.allocationMisses)
			return fail("graph edit did not invalidate the cached allocation plan");
		auto cacheBeforeResize = valid.getPlanCacheStats();
		auto resizedAllocation = valid.buildAllocationPlan({ 321, 180 });
		if (!resizedAllocation.valid || valid.getPlanCacheStats().allocationMisses <= cacheBeforeResize.allocationMisses) return fail("new graph viewport reused an allocation plan for different dimensions");
		RenderGraph cacheCopy(valid);
		if (cacheCopy.getPlanCacheStats().compileHits || cacheCopy.getPlanCacheStats().compileMisses || !cacheCopy.compile().valid || cacheCopy.getPlanCacheStats().compileMisses != 1)
			return fail("RenderGraph copy inherited another graph's plan cache");
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

		// Loading a transient image that nothing produced earlier reads whatever the
		// allocator's aliasing left behind, so the result changes with an allocation
		// decision rather than with the graph.
		RenderGraph loadTransient;
		auto scratch = loadTransient.createImage("Scratch", colour);
		auto loader = loadTransient.addPass("Loader", GraphPassType::Fullscreen);
		loadTransient.writeColour(loader, scratch, GraphLoadOp::Load);
		if (loadTransient.compile().valid) return fail("loading an unproduced transient image was accepted");
		// The same load is well defined once an earlier pass has produced it.
		RenderGraph loadProduced;
		auto produced = loadProduced.createImage("Produced", colour);
		auto firstWriter = loadProduced.addPass("FirstWriter", GraphPassType::Fullscreen);
		produced = loadProduced.writeColour(firstWriter, produced, GraphLoadOp::Clear);
		auto secondWriter = loadProduced.addPass("SecondWriter", GraphPassType::Fullscreen);
		loadProduced.writeColour(secondWriter, produced, GraphLoadOp::Load);
		if (!loadProduced.compile().valid) return fail("loading a transient image produced by an earlier pass was rejected");
		// And on a non-transient image, whose contents the allocator must preserve.
		GraphImageDesc persistentColour = colour; persistentColour.transient = false;
		RenderGraph loadPersistent;
		auto kept = loadPersistent.createImage("Kept", persistentColour);
		auto keptLoader = loadPersistent.addPass("KeptLoader", GraphPassType::Fullscreen);
		loadPersistent.writeColour(keptLoader, kept, GraphLoadOp::Load);
		if (!loadPersistent.compile().valid) return fail("loading a non-transient image was rejected");
		// DontCare says the contents do not matter, which is exactly the honest
		// declaration for an unproduced transient image.
		RenderGraph dontCareTransient;
		auto ignored = dontCareTransient.createImage("Ignored", colour);
		auto ignoringPass = dontCareTransient.addPass("Ignoring", GraphPassType::Fullscreen);
		dontCareTransient.writeColour(ignoringPass, ignored, GraphLoadOp::DontCare);
		if (!dontCareTransient.compile().valid) return fail("a DontCare write to an unproduced transient image was rejected");

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

		// The plan and RenderGraphTargets used to carry separate compatibility
		// predicates, and the plan's ignored six sampler fields plus usage. Both now
		// call graphImagesCanAlias, so a descriptor difference the real allocator
		// respects must stop the plan grouping too. Three ends of the same graph:
		// identical descriptors alias, a sampler difference does not, a usage
		// difference does not.
		auto planEndImagesTogether = [&](GraphImageDesc const& tailDesc)
		{
			RenderGraph graph;
			auto head = graph.createImage("Head", colour);
			auto middle = graph.createImage("Middle", colour);
			auto tail = graph.createImage("Tail", tailDesc);
			auto step0 = graph.addPass("Step0", GraphPassType::Fullscreen);
			head = graph.writeColour(step0, head);
			auto step1 = graph.addPass("Step1", GraphPassType::Fullscreen);
			graph.readSampled(step1, head); middle = graph.writeColour(step1, middle);
			auto step2 = graph.addPass("Step2", GraphPassType::Fullscreen);
			graph.readSampled(step2, middle); graph.writeColour(step2, tail);
			auto plan = graph.buildAllocationPlan({ 32, 32 });
			if (!plan.valid) return false;
			uint32_t headAllocation = UINT32_MAX, tailAllocation = UINT32_MAX;
			for (auto const& lifetime : plan.allocatedImages)
			{
				if (lifetime.image.id == head.id) headAllocation = lifetime.physicalAllocation;
				if (lifetime.image.id == tail.id) tailAllocation = lifetime.physicalAllocation;
			}
			return headAllocation != UINT32_MAX && headAllocation == tailAllocation;
		};
		if (!planEndImagesTogether(colour))
			return fail("identical disjoint graph images were not planned onto one allocation");
		GraphImageDesc biased = colour; biased.params.lodBias = 1.5f;
		if (planEndImagesTogether(biased))
			return fail("graph images differing only in sampler LOD bias were planned onto one allocation");
		GraphImageDesc unsampled = colour; unsampled.usage = GraphImageUsage::ColourAttachment;
		if (planEndImagesTogether(unsampled))
			return fail("graph images differing in declared usage were planned onto one allocation");

		// A bloom blur pass used to decide which blur level it was by parsing digits
		// off the end of its own name, so renaming it in the editor silently changed
		// how many levels the blurPasses option enabled. It now reads an authored
		// ITERATION parameter, and a graph that omits one is reported rather than
		// quietly falling back.
		auto const* blurMetadata = registry.findMetadata("MPP.BloomBlurHorizontal");
		if (!blurMetadata || blurMetadata->nameDerivedFallbackParameter != "ITERATION" ||
			std::find_if(blurMetadata->parameters.begin(), blurMetadata->parameters.end(),
				[](GraphPassParameterMetadata const& parameter) { return parameter.name == "ITERATION" && parameter.type == program::GLSLType::Int; }) == blurMetadata->parameters.end())
			return fail("bloom blur metadata does not declare an ITERATION parameter");
		UniformCollection authoredIteration; authoredIteration.setUniform("ITERATION", (int32_t)2);
		if (bloomBlurIteration(authoredIteration, "Blur - wide") != 2)
			return fail("a bloom blur pass ignored its authored ITERATION parameter");
		if (bloomBlurIteration({}, "BloomBlurHorizontal3") != 3)
			return fail("a bloom blur pass without ITERATION lost the name-derived fallback");
		if (bloomBlurIteration({}, "Blur - wide") != 0)
			return fail("a bloom blur pass with neither ITERATION nor trailing digits did not fall back to zero");
		auto buildBlurGraph = [&](bool declareIteration)
		{
			RenderGraph graph;
			auto source = graph.createImage("BlurSource", colour);
			auto blurred = graph.createImage("BlurTarget", colour);
			// Disabled so registry validation ignores it: it carries no factory, and
			// only the blur pass is under test here.
			auto producer = graph.addPass("BlurProducer", GraphPassType::Fullscreen);
			source = graph.writeColour(producer, source);
			graph.setPassEnabled(producer, false);
			// Deliberately a name with no trailing digits, which is exactly what the
			// old fallback cannot read.
			auto blurPass = graph.addPass("Blur - wide", GraphPassType::Fullscreen);
			graph.setPassCallbackFactory(blurPass, "MPP.BloomBlurHorizontal");
			graph.bindSampler(blurPass, "TEX1", source);
			graph.writeColour(blurPass, blurred);
			if (declareIteration) { UniformCollection parameters; parameters.setUniform("ITERATION", (int32_t)2); graph.setPassParameters(blurPass, parameters); }
			return graph;
		};
		auto declaredBlur = registry.validate(buildBlurGraph(true));
		if (declaredBlur.hasErrors()) return fail("a bloom blur pass declaring ITERATION was rejected");
		for (auto const& diagnostic : declaredBlur.getDiagnostics())
			if (diagnostic.code == "MPP-PASS-013") return fail("a bloom blur pass declaring ITERATION was still reported as name-derived");
		// Bind the bag to a local. Iterating registry.validate(...).getDiagnostics()
		// directly destroys the temporary DiagnosticBag before the loop body runs,
		// which silently yields no diagnostics at all.
		auto const inferredBlur = registry.validate(buildBlurGraph(false));
		bool reportedNameFallback = false;
		for (auto const& diagnostic : inferredBlur.getDiagnostics())
			if (diagnostic.code == "MPP-PASS-013") reportedNameFallback = true;
		if (!reportedNameFallback) return fail("a bloom blur pass without ITERATION was not reported as deriving it from its name");

		// compile(Caps) used to contain an empty per-image loop, so no image was
		// checked against device limits and PbrPipelineDocument::validate reported a
		// pipeline as valid that then threw at allocatePhysical on the device.
		Caps limitedCaps{}; limitedCaps.maxTextureSize = 256; limitedCaps.maxColourAttachments = 4; limitedCaps.maxDrawBuffers = 4;
		auto compileWithImage = [&](GraphImageDesc const& desc, glm::uvec2 const& viewport)
		{
			RenderGraph graph;
			auto image = graph.createImage("Limited", desc);
			auto pass = graph.addPass("LimitedPass", GraphPassType::Fullscreen);
			graph.writeColour(pass, image);
			return viewport.x ? graph.compile(limitedCaps, viewport) : graph.compile(limitedCaps);
		};
		GraphImageDesc oversized = colour; oversized.absoluteSize = { 512, 512 }; oversized.relativeSize = { 0.0f, 0.0f };
		if (compileWithImage(oversized, {}).valid)
			return fail("an image larger than the maximum texture size compiled against caps");
		GraphImageDesc sized = colour; sized.absoluteSize = { 64, 64 }; sized.relativeSize = { 0.0f, 0.0f };
		if (!compileWithImage(sized, {}).valid)
			return fail("an image within the maximum texture size was rejected");
		GraphImageDesc overMipped = sized; overMipped.mipLevels = 12;
		if (compileWithImage(overMipped, {}).valid)
			return fail("an image declaring more mip levels than its size supports compiled against caps");
		// The whole point of the viewport overload: a relative image is unresolvable
		// without one, so the no-viewport call must accept it and the viewport call
		// must catch it.
		GraphImageDesc relative = colour; relative.absoluteSize = { 0, 0 }; relative.relativeSize = { 1.0f, 1.0f };
		if (!compileWithImage(relative, {}).valid)
			return fail("a viewport-relative image was rejected without a viewport to resolve it");
		if (compileWithImage(relative, { 512, 512 }).valid)
			return fail("a viewport-relative image exceeding the maximum texture size compiled against caps");
		if (!compileWithImage(relative, { 128, 128 }).valid)
			return fail("a viewport-relative image within the maximum texture size was rejected");

		RenderGraph capabilityCached;
		auto capabilityImage = capabilityCached.createImage("CapabilityCached", relative);
		auto capabilityPass = capabilityCached.addPass("CapabilityPass", GraphPassType::Fullscreen);
		capabilityCached.writeColour(capabilityPass, capabilityImage);
		if (capabilityCached.compile(limitedCaps, { 512, 512 }).valid) return fail("capability cache test did not reject the limited device");
		auto cacheAfterLimitedCompile = capabilityCached.getPlanCacheStats();
		if (capabilityCached.compile(limitedCaps, { 512, 512 }).valid || capabilityCached.getPlanCacheStats().compileHits <= cacheAfterLimitedCompile.compileHits)
			return fail("unchanged capability compilation did not hit the plan cache");
		auto largerCaps = limitedCaps; largerCaps.maxTextureSize = 1024;
		auto cacheBeforeLargerDevice = capabilityCached.getPlanCacheStats();
		if (!capabilityCached.compile(largerCaps, { 512, 512 }).valid || capabilityCached.getPlanCacheStats().compileMisses <= cacheBeforeLargerDevice.compileMisses)
			return fail("capability compilation reused a plan for a different device signature");

		return true;
	}
}
