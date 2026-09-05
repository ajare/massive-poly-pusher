#include "ParticleScene.h"

#include <array>
#include <format>
#include <stdexcept>
#include <tuple>

#include <glm/gtc/matrix_transform.hpp>

#include <mpp/ParticleEffectSpecification.h>
#include <mpp/ProgrammaticParticleEffectStream.h>
#include <mpp/ProgrammaticRenderTextureStream.h>
#include <mpp/ProgrammaticTextureStream.h>
#include <mpp/RenderGraphStream.h>
#include <mpp/RenderPipeline.h>
#include <mpp/resource-parsers/FileRenderGraphStream.h>

namespace
{
	mpp::ParticleEmitterTemplate emitterTemplate(
		mpp::ParticleSpawnShape shape, mpp::ParticleBlendClass blend,
		glm::vec3 position, uint32_t budget)
	{
		mpp::ParticleEmitterTemplate result;
		result.simulation.shapeSeedModulesBudget = { uint32_t(shape), uint32_t(shape) * 7919u + 17u, 0u, budget };
		result.simulation.emissionState = { 0u, 1u, 0u, 0u };
		result.simulation.emissionRateAndPadding[0] = 180.0f;
		result.simulation.lifetimeSizeRanges = { 2.0f, 4.0f, 0.35f, 0.8f };
		result.simulation.initialVelocityMin = { -0.15f, 0.25f, -0.15f, 0.0f };
		result.simulation.initialVelocityMax = { 0.15f, 1.1f, 0.15f, 0.0f };
		result.simulation.colourMin = { 0.35f, 0.65f, 1.0f, 0.45f };
		result.simulation.colourMax = { 1.0f, 0.55f, 0.2f, 0.9f };
		result.appearance.appearance = { blend == mpp::ParticleBlendClass::Additive ? 2.5f : 0.7f, 0.8f, 0.0f, 0.0f };
		result.appearance.modes[3] = uint32_t(blend);
		result.localTransform = glm::translate(glm::mat4(1.0f), position);
		result.curves[size_t(mpp::ParticleScalarCurve::Size)].keys = { { 0.0f, 0.25f }, { 0.2f, 1.0f }, { 1.0f, 0.1f } };
		result.curves[size_t(mpp::ParticleScalarCurve::Alpha)].keys = { { 0.0f, 0.0f }, { 0.12f, 1.0f }, { 0.8f, 0.8f }, { 1.0f, 0.0f } };
		return result;
	}

	mpp::SceneDocument demoSceneDocument()
	{
		mpp::SceneDocument document;
		document.name = "Particle Demo";
		document.sourcePath = "ParticleScene.procedural";
		document.camera.position = { 0.0f, 4.2f, 10.0f };
		document.camera.target = { 0.0f, 0.7f, 0.0f };
		document.camera.nearPlane = 0.1f;
		document.camera.farPlane = 100.0f;

		mpp::SceneModelDocument grid;
		grid.id = "intersection-grid";
		grid.source = mpp::SceneModelSource::Grid;
		grid.primitive.width = 12.0f;
		grid.primitive.depth = 12.0f;
		grid.primitive.segmentsX = 24;
		grid.primitive.segmentsZ = 24;
		document.models.push_back(grid);

		mpp::SceneModelDocument box;
		box.id = "intersection-box";
		box.source = mpp::SceneModelSource::Box;
		box.primitive.width = 2.0f;
		box.primitive.height = 1.0f;
		box.primitive.depth = 2.0f;
		box.translation = { -2.2f, 0.5f, 0.0f };
		document.models.push_back(box);

		mpp::SceneModelDocument sphere;
		sphere.id = "intersection-sphere";
		sphere.source = mpp::SceneModelSource::Sphere;
		sphere.primitive.radius = 1.0f;
		sphere.primitive.resolution = 3;
		sphere.translation = { 2.2f, 1.0f, 0.0f };
		document.models.push_back(sphere);

		mpp::SceneLightDocument light;
		light.id = "sun";
		light.type = mpp::SceneLightType::Directional;
		light.direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.35f));
		light.intensity = 3.0f;
		document.lights.push_back(light);
		return document;
	}
}

ParticleScene::ParticleScene(mpp::ResourceManager* resources, std::filesystem::path resourceRoot)
	: Scene("Default", resources)
	, mResourceRoot(std::move(resourceRoot))
{
}

mpp::CameraPtr ParticleScene::createCamera(ProgramOptions const& options) const
{
	auto camera = std::make_shared<mpp::Camera>(glm::vec3(0.0f, 4.2f, 10.0f), 0.0f, 0.0f, 0.0f,
		55.0f, float(options.screenWidth) / options.screenHeight);
	camera->setLookAt({ 0.0f, 4.2f, 10.0f }, { 0.0f, 0.7f, 0.0f });
	camera->setClipDistances(0.1f, 100.0f);
	return camera;
}

void ParticleScene::setupImpl(mpp::RenderSystem* renderer, ProgramOptions const& options)
{
	mRenderer = renderer;
	auto presentationStream = std::make_shared<mpp::ProgrammaticRenderTextureStream>(getResourceManager());
	presentationStream->setTarget(mpp::TextureTarget::Texture2D);
	presentationStream->setInternalFormat(mpp::TextureInternalType::UnsignedInteger, true, 8u, 4u);
	presentationStream->setWidth(options.screenWidth);
	presentationStream->setHeight(options.screenHeight);
	presentationStream->setNumAttachments(1u);
	presentationStream->setFiltering(mpp::TextureParams::MinFilter::Linear, mpp::TextureParams::MagFilter::Linear);
	auto presentationResource = getResourceManager()->declareResource("DemoSuite.ParticlePresentation", presentationStream).first;
	presentationResource->load();
	mPresentationTarget = std::dynamic_pointer_cast<mpp::RenderTexture>(presentationResource);
	if (!mPresentationTarget) throw std::runtime_error("Could not create the particle presentation target.");

	for (auto const& [name, file, mode] : std::array{
		std::tuple{ std::string("ParticlePbr"), "ParticlePbr.rendergraph.yaml", mpp::RenderPipelineMode::XmlGraphPbrForward },
		std::tuple{ std::string("ParticleLegacy"), "ParticleLegacy.rendergraph.yaml", mpp::RenderPipelineMode::GraphLegacyForward } })
	{
		auto stream = std::make_shared<mpp::resource_parsers::FileRenderGraphStream>(
			getResourceManager(), (mResourceRoot / file).string());
		auto graph = getResourceManager()->declareResource("DemoSuite." + name + ".Graph", stream).first;
		graph->load();
		mpp::RenderPipelineOptions pipelineOptions;
		pipelineOptions.mode = mode;
		pipelineOptions.graphTemplate = graph;
		pipelineOptions.graphImports["screen"] = mPresentationTarget;
		renderer->getOrCreateRenderPipeline(name, pipelineOptions);
	}

	mSceneRuntime = std::make_unique<mpp::SceneRuntime>(renderer, getResourceManager());
	if (!mSceneRuntime->rebuild(demoSceneDocument(), {}, {}, {}, {}))
		throw std::runtime_error("Could not create the procedural particle demo scene.");

	getCamera()->setAspectRatio(float(options.screenWidth) / options.screenHeight);
	auto& particles = renderer->getParticleSystem();
	particles.initialise();
	particles.setStatisticsEnabled(true);
	createDemoEffect();
}

void ParticleScene::createDemoEffect()
{
	auto textureStream = std::make_shared<mpp::ProgrammaticTextureStream>(getResourceManager());
	textureStream->setTarget(mpp::TextureTarget::Texture2D);
	textureStream->setFile((mResourceRoot / "atlas.png").string(), getResourceManager()->getImageLoadFunction());
	textureStream->setColourSpace(mpp::TextureColourSpace::Srgb);
	textureStream->setFiltering(mpp::TextureParams::MinFilter::Linear, mpp::TextureParams::MagFilter::Linear);
	textureStream->setWrapping(mpp::TextureParams::Wrapping::ClampToEdge);
	auto atlas = getResourceManager()->declareResource("DemoSuite.ParticleAtlas", textureStream).first;
	atlas->load();

	mpp::ParticleEffectSpecification specification;
	specification.name = "DemoSuite particle showcase";
	auto cone = emitterTemplate(mpp::ParticleSpawnShape::Cone, mpp::ParticleBlendClass::Additive, { -2.2f, -0.05f, 0.0f }, 12000u);
	cone.simulation.shapeParameters = { 0.8f, 0.35f, 0.0f, 0.0f };
	cone.simulation.shapeSeedModulesBudget[2] = uint32_t(mpp::ParticleBehaviourModule::Gravity);
	cone.simulation.gravityAndDrag = { 0.0f, -0.35f, 0.0f, 0.0f };
	cone.appearance.modes[2] = uint32_t(mpp::ParticleBillboardMode::VelocityStretched);
	specification.emitterTemplates.push_back({ "additive-cone", cone, {} });

	auto disc = emitterTemplate(mpp::ParticleSpawnShape::Disc, mpp::ParticleBlendClass::Additive, { 0.0f, 0.02f, -2.0f }, 12000u);
	disc.simulation.shapeParameters[0] = 1.1f;
	disc.simulation.emissionRateAndPadding[0] = 70.0f;
	disc.appearance.tintAndAlpha = { 1.0f, 0.4f, 0.1f, 0.8f };
	specification.emitterTemplates.push_back({ "additive-disc", disc, {} });

	auto mist = emitterTemplate(mpp::ParticleSpawnShape::Box, mpp::ParticleBlendClass::Alpha, { 0.0f, 0.12f, 0.0f }, 12000u);
	mist.simulation.shapeParameters = { 1.8f, 0.18f, 1.8f, 0.0f };
	mist.simulation.emissionRateAndPadding[0] = 120.0f;
	mist.simulation.initialVelocityMin = { -0.05f, 0.02f, -0.05f, 0.0f };
	mist.simulation.initialVelocityMax = { 0.05f, 0.18f, 0.05f, 0.0f };
	mist.appearance.tintAndAlpha = { 0.65f, 0.8f, 1.0f, 0.55f };
	specification.emitterTemplates.push_back({ "alpha-box-mist", mist, {} });

	auto flipbook = emitterTemplate(mpp::ParticleSpawnShape::Sphere, mpp::ParticleBlendClass::Alpha, { 2.2f, 0.85f, 0.0f }, 12000u);
	flipbook.simulation.shapeParameters[0] = 0.75f;
	flipbook.simulation.emissionRateAndPadding[0] = 35.0f;
	flipbook.appearance.textureAndAtlas = { 0u, 0u, 2u, 2u };
	flipbook.appearance.appearance[2] = 6.0f;
	flipbook.appearance.modes = { 4u, uint32_t(mpp::ParticleTextureAnimation::FixedRate), uint32_t(mpp::ParticleBillboardMode::CameraFacing), uint32_t(mpp::ParticleBlendClass::Alpha) };
	specification.emitterTemplates.push_back({ "alpha-flipbook-sphere", flipbook, atlas->getName() });

	auto effectStream = std::make_shared<mpp::ProgrammaticParticleEffectStream>(getResourceManager());
	effectStream->setSpecification(specification);
	auto effect = getResourceManager()->declareResource("DemoSuite.ParticleEffect", effectStream).first;
	effect->load();
	mDemoEffect = mRenderer->getParticleSystem().createEffect(effect);
}

void ParticleScene::enableStressMode()
{
	if (mStressMode || !mRenderer) return;
	mStressMode = true;
	auto const capacity = mRenderer->getParticleSystem().getPoolCapacity();
	std::array<mpp::ParticleEmitterTemplate, 3> emitters;
	uint32_t assigned = 0;
	for (uint32_t index = 0; index < emitters.size(); ++index)
	{
		uint32_t const budget = index + 1u == emitters.size() ? capacity - assigned : capacity / uint32_t(emitters.size());
		assigned += budget;
		emitters[index] = emitterTemplate(mpp::ParticleSpawnShape::Point, mpp::ParticleBlendClass::Alpha,
			{ (float(index) - 1.0f) * 2.0f, 0.1f, -1.0f }, budget);
		emitters[index].simulation.emissionState = { 1u, 1u, capacity, 0u };
		emitters[index].simulation.lifetimeSizeRanges = { 30.0f, 30.0f, 0.025f, 0.045f };
		emitters[index].simulation.initialVelocityMin = { -0.25f, 0.1f, -0.25f, 0.0f };
		emitters[index].simulation.initialVelocityMax = { 0.25f, 0.8f, 0.25f, 0.0f };
	}
	mStressEffect = mRenderer->getParticleSystem().createEffect(emitters);
}

void ParticleScene::teardownImpl()
{
	if (mRenderer)
	{
		auto& particles = mRenderer->getParticleSystem();
		if (mDemoEffect) particles.destroyEffect(mDemoEffect);
		if (mStressEffect) particles.destroyEffect(mStressEffect);
		particles.setStatisticsEnabled(false);
		mRenderer->removeRenderPipeline("ParticlePbr");
		mRenderer->removeRenderPipeline("ParticleLegacy");
	}
	mSceneRuntime.reset();
	mPresentationTarget.reset();
	for (auto const* name : { "DemoSuite.ParticleEffect", "DemoSuite.ParticleAtlas", "DemoSuite.ParticlePresentation", "DemoSuite.ParticlePbr.Graph", "DemoSuite.ParticleLegacy.Graph" })
		if (getResourceManager()->getResource(name, true)) getResourceManager()->deleteResourceTree(name);
	mRenderer = nullptr;
}

std::string ParticleScene::getRenderPipelineName() const
{
	return mPipelineName;
}

void ParticleScene::handleInput(InputManager* input)
{
	if (input->keyPressed(Key_P))
	{
		mPbr = !mPbr;
		mPipelineName = mPbr ? "ParticlePbr" : "ParticleLegacy";
		getCamera()->markCut();
	}
	if (input->keyPressed(Key_Space)) enableStressMode();
}

void ParticleScene::render(mpp::RenderSystem* renderer, World const&, RenderOptions const&)
{
	if (mSceneRuntime) renderer->renderScene(mSceneRuntime->getScene(), getCamera(), glm::vec2(0.0f), mPipelineName);
}

void ParticleScene::present(mpp::RenderSystem* renderer)
{
	auto presentationTexture = std::dynamic_pointer_cast<mpp::RenderTexture>(mPresentationTarget);
	if (!presentationTexture) throw std::runtime_error("Particle presentation target is not a render texture.");
	mpp::RenderSystem::TextureDiagnosticOptions options;
	renderer->renderTextureDiagnostic(presentationTexture.get(), renderer->getScreenRenderTarget(), options);
}

std::vector<std::string> ParticleScene::getOverlayLines() const
{
	std::vector<std::string> lines{
		std::format("P: graph [{}] (toggle PBR / legacy)", mPbr ? "PBR" : "legacy"),
		std::format("Space: stress mode [{}]", mStressMode ? "active" : "ready")
	};
	if (!mRenderer) return lines;
	auto const& stats = mRenderer->getParticleSystem().getStats();
	if (!stats.valid)
	{
		lines.push_back("Particles: counters pending...");
		return lines;
	}
	lines.push_back(std::format("Particles: {} / {} ({:.1f}%), free {}, emitters {}", stats.activeParticles,
		stats.capacity, stats.capacityUsage * 100.0f, stats.freeParticles, stats.activeEmitters));
	lines.push_back(std::format("Spawned {}  killed {}  dropped/exhausted {}", stats.spawnedParticles,
		stats.killedParticles, stats.droppedParticles));
	lines.push_back(std::format("Rendered {}  culled {}  GPU sim {:.3f} ms  draw {:.3f} ms  lag {} frames",
		stats.renderedParticles, stats.culledParticles, stats.simulationGpuMilliseconds,
		stats.renderGpuMilliseconds, stats.framesLagged));
	return lines;
}
