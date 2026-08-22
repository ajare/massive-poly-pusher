#include <filesystem>
#include <fstream>

#include "mpp/PbrPipelineDocument.h"
#include "mpp/resource-parsers/PbrPipelineParser.h"
#include "mpp/resource-parsers/PbrPipelineSerializer.h"
#include "mpp/resource-parsers/RenderGraphParser.h"
#include "mpp/resource-parsers/RenderGraphResourceTests.h"
#include "mpp/resource-parsers/RenderGraphSerializer.h"

namespace mpp::resource_parsers
{
	namespace
	{
		bool runForExtension(std::string const& extension, std::string* failure)
		{
			auto fail = [&](std::string const& message) { if (failure) *failure = message; return false; };
			auto const root = std::filesystem::temp_directory_path() / ("mpp_render_graph_resource_test" + extension);
			auto const bootstrapXml = root.string() + "_bootstrap.xml";
			auto const source = root.string() + "_source" + extension;
			auto const roundTrip = root.string() + "_roundtrip" + extension;
			{
				std::ofstream file(bootstrapXml);
				file << R"(<RenderGraph>
<Images>
 <Image><name>Input</name><format>RGBA16F</format><usage>sampled,colourAttachment</usage><width>32</width><height>16</height><mipLevels>3</mipLevels><colourSpace>LINEAR</colourSpace><minFilter>LINEAR</minFilter><magFilter>LINEAR</magFilter><wrap>CLAMP_TO_EDGE</wrap></Image>
 <Image><name>Screen</name><format>RGBA8</format><usage>colourAttachment,presentation</usage><import>screen</import><external>true</external></Image>
</Images>
<Passes>
 <Pass><name>Effect</name><type>fullscreen</type><program>Effects.Test</program><factory>Tests.Effect</factory><Inputs><Sampled><sampler>TEX1</sampler><mipLevel>1</mipLevel><image>Input</image></Sampled></Inputs><Parameters><Float><name>EXPOSURE</name><value>1.5</value></Float><Vec3><name>TINT</name><value>1 0.5 0.25</value></Vec3></Parameters><Colours><Output><image>Input</image><mipLevel>2</mipLevel><load>clear</load><store>store</store><clear>0 0 0 1</clear></Output></Colours><Raster><explicit>true</explicit><fill>line</fill><frontFace>clockwise</frontFace><cull>front</cull><depthTest>false</depthTest><depthWrite>false</depthWrite><depthCompare>greaterEqual</depthCompare><blend>true</blend><colourBlendOp>reverseSubtract</colourBlendOp><alphaBlendOp>maximum</alphaBlendOp><sourceColourBlend>sourceAlpha</sourceColourBlend><destinationColourBlend>oneMinusSourceAlpha</destinationColourBlend><sourceAlphaBlend>one</sourceAlphaBlend><destinationAlphaBlend>oneMinusDestinationAlpha</destinationAlphaBlend><multisample>false</multisample><alphaToCoverage>true</alphaToCoverage><scissor>true</scissor><scissorRectangle>4 8 16 32</scissorRectangle><ColourWriteMasks><Mask>true false true false</Mask></ColourWriteMasks></Raster></Pass>
 <Pass><name>Present</name><type>present</type><Inputs><Sampled><image>Input</image></Sampled></Inputs><Colours><Output><image>Screen</image><load>dontCare</load><store>store</store></Output></Colours></Pass>
</Passes>
</RenderGraph>)";
			}
			try
			{
				// The fixture text above is XML; for the YAML leg, bootstrap the real
				// source file by parsing it once and re-serializing through the same
				// writer path production code uses, instead of hand-authoring a
				// second fixture that would drift from the collapse scheme.
				if (extension == ".xml")
				{
					std::filesystem::copy_file(bootstrapXml, source, std::filesystem::copy_options::overwrite_existing);
				}
				else
				{
					auto bootstrapGraph = RenderGraphParser::fromFile(bootstrapXml);
					RenderGraphSerializer::toFile(bootstrapGraph, source);
				}

				auto graph = RenderGraphParser::fromFile(source);
				if (graph.getImageCount() != 2 || graph.getPassCount() != 2) return fail(extension + ": parsed graph counts differ");
				auto screen = graph.getImageInfo({ 1, 0 });
				if (!screen.desc.external || screen.importName != "screen") return fail(extension + ": named import was not parsed");
				auto effect = graph.getPassInfo({ 0 });
				if (effect.type != GraphPassType::Fullscreen || effect.programResource != "Effects.Test" || effect.samplerBindings.size() != 1 || effect.samplerBindings.front().mipLevel != 1 || effect.parameters.getNumUniforms() != 2 || effect.colourOutputs.front().mipLevel != 2) return fail(extension + ": pass metadata was not parsed");

				// Raster state is authored in the editor and honoured by the executor, so
				// losing it in either direction silently discards the author's work.
				GraphRasterState expectedRaster;
				expectedRaster.explicitState = true;
				expectedRaster.fillMode = GraphFillMode::Line;
				expectedRaster.frontFace = GraphFrontFace::Clockwise;
				expectedRaster.cullMode = GraphCullMode::Front;
				expectedRaster.depthTest = false;
				expectedRaster.depthWrite = false;
				expectedRaster.depthCompare = GraphCompareOp::GreaterEqual;
				expectedRaster.blend = true;
				expectedRaster.colourBlendOp = GraphBlendOp::ReverseSubtract;
				expectedRaster.alphaBlendOp = GraphBlendOp::Maximum;
				expectedRaster.sourceColourBlend = GraphBlendFactor::SourceAlpha;
				expectedRaster.destinationColourBlend = GraphBlendFactor::OneMinusSourceAlpha;
				expectedRaster.sourceAlphaBlend = GraphBlendFactor::One;
				expectedRaster.destinationAlphaBlend = GraphBlendFactor::OneMinusDestinationAlpha;
				expectedRaster.multisample = false;
				expectedRaster.alphaToCoverage = true;
				expectedRaster.scissor = true;
				expectedRaster.scissorRectangle = { 4, 8, 16, 32 };
				expectedRaster.colourWriteMasks = { { true, false, true, false } };
				if (!(effect.rasterState == expectedRaster)) return fail(extension + ": pass raster state was not parsed");
				if (!(graph.getPassInfo({ 1 }).rasterState == GraphRasterState{})) return fail(extension + ": a pass without a Raster block did not keep the default state");

				RenderGraphSerializer::toFile(graph, roundTrip);
				auto restored = RenderGraphParser::fromFile(roundTrip);
				auto restoredEffect = restored.getPassInfo({ 0 });
				if (restored.getImageCount() != 2 || restoredEffect.programResource != "Effects.Test" || restoredEffect.samplerBindings.size() != 1 || restoredEffect.samplerBindings.front().mipLevel != 1 || restoredEffect.parameters.getNumUniforms() != 2 || restoredEffect.colourOutputs.front().mipLevel != 2) return fail(extension + ": graph round trip lost metadata");
				if (!(restoredEffect.rasterState == expectedRaster)) return fail(extension + ": graph round trip lost raster state");
				if (!(restored.getPassInfo({ 1 }).rasterState == GraphRasterState{})) return fail(extension + ": graph round trip invented raster state for a default pass");

				// A default state must not be emitted at all, so documents that never
				// touched raster state stay byte-identical through the editor.
				{
					std::ifstream restoredFile(roundTrip);
					std::string const text((std::istreambuf_iterator<char>(restoredFile)), std::istreambuf_iterator<char>());
					std::string const rasterMarker = extension == ".xml" ? "<Raster>" : "Raster:";
					if (text.find(rasterMarker) == std::string::npos) return fail(extension + ": explicit raster state was not serialized");
					if (text.find(rasterMarker) != text.rfind(rasterMarker)) return fail(extension + ": default raster state was serialized");
				}
			}
			catch (std::exception const& exception)
			{
				return fail(extension + ": " + exception.what());
			}
			std::filesystem::remove(bootstrapXml);
			std::filesystem::remove(source);
			std::filesystem::remove(roundTrip);
			return true;
		}
	}

		bool runPbrPipelineSsaoDocumentTest(std::string const& extension, std::string* failure)
		{
			auto fail = [&](std::string const& message) { if (failure) *failure = message; return false; };
			auto const path = (std::filesystem::temp_directory_path() / ("mpp_ssao_pipeline_document" + extension)).string();
			try
			{
				PbrPipelineDocument document;
				document.name = "SSAO structural test";
				document.graph = std::make_shared<RenderGraph>();
				GraphImageDesc colour; colour.format = GraphImageFormat::Rgba16f; colour.usage = GraphImageUsage::ColourAttachment | GraphImageUsage::Sampled;
				GraphImageDesc depth; depth.format = GraphImageFormat::Depth24; depth.usage = GraphImageUsage::DepthAttachment;
				auto scene = document.graph->createImage("SceneHdr", colour);
				auto sceneDepth = document.graph->createImage("SceneDepth", depth);
				auto bloom = document.graph->createImage("BloomExtract", colour);
				auto scenePass = document.graph->addPass("PbrScene", GraphPassType::Scene);
				document.graph->setPassCallbackFactory(scenePass, "MPP.PbrScene");
				scene = document.graph->writeColour(scenePass, scene, GraphLoadOp::Clear, GraphStoreOp::Store);
				document.graph->writeDepth(scenePass, sceneDepth, GraphLoadOp::Clear, GraphStoreOp::DontCare);
				auto bloomPass = document.graph->addPass("BloomExtract", GraphPassType::Fullscreen);
				document.graph->setPassCallbackFactory(bloomPass, "MPP.FullscreenEffect");
				document.graph->setPassProgramResource(bloomPass, "PostEffect.BloomExtract");
				document.graph->bindSampler(bloomPass, "TEX1", scene);
				document.graph->writeColour(bloomPass, bloom);
				document.ssao = { true, 0.7f, 1.3f, 0.04f, 1.2f, 24, 3 };
				document.setSSAOEnabled(true);
				auto const pass = [&](uint32_t index) { return document.graph->getPassInfo({ index }); };
				if (document.graph->getPassCount() != 5 || pass(1).name != "SSAO" || pass(2).name != "SSAOBlur" || pass(3).name != "SSAOComposite" || pass(4).name != "BloomExtract") return fail(extension + ": SSAO passes were not inserted between the scene and bloom extract passes");
				if (document.graph->getImageCount() != 6 || pass(4).samplerBindings.front().image.id == scene.id) return fail(extension + ": SSAO images or bloom routing were not authored");
				if (!hasGraphImageUsage(document.graph->getImageInfo({ sceneDepth.id, 0 }).desc.usage, GraphImageUsage::Sampled) || document.graph->getPassInfo(scenePass).depthOutputs.front().store != GraphStoreOp::Store) return fail(extension + ": SSAO did not retain sampled scene depth");
				PbrPipelineSerializer::toFile(document, path);
				auto restored = PbrPipelineParser::fromFile(path);
				if (!restored.ssao.enabled || restored.ssao.radius != 0.7f || restored.ssao.intensity != 1.3f || restored.ssao.bias != 0.04f || restored.ssao.power != 1.2f || restored.ssao.sampleCount != 24 || restored.ssao.blurRadius != 3) return fail(extension + ": SSAO options did not survive pipeline round trip");
				restored.setSSAOEnabled(false);
				if (restored.graph->getPassCount() != 2 || restored.graph->getImageCount() != 3) return fail(extension + ": disabling SSAO did not remove its authored passes and images");
			}
			catch (std::exception const& exception) { return fail(extension + ": " + exception.what()); }
			std::filesystem::remove(path);
			return true;
		}

	bool runRenderGraphResourceTests(std::string* failure)
	{
		for (auto const& extension : { std::string(".xml"), std::string(".yaml") })
		{
			if (!runForExtension(extension, failure)) return false;
			if (!runPbrPipelineSsaoDocumentTest(extension, failure)) return false;
		}
		return true;
	}
}
