#include "PackageScene.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include <mpp/RenderGraphStream.h>
#include <mpp/RenderPipeline.h>
#include <mpp/app/PackageManifest.h>
#include <mpp/helper/FpsCamera.h>
#include <mpp/resource-parsers/PbrPipelineDocumentLoader.h>
#include <mpp/resource-parsers/SceneParser.h>

namespace
{
	std::string diagnosticsSummary(mpp::DiagnosticBag const& diagnostics)
	{
		std::string result;
		for (auto const& diagnostic : diagnostics.getDiagnostics())
		{
			if (diagnostic.severity != mpp::DiagnosticSeverity::Error)
			{
				continue;
			}
			if (!result.empty())
			{
				result += '\n';
			}
			result += "[" + diagnostic.code + "] " + diagnostic.message;
		}
		return result;
	}
}

PackageScene::PackageScene(mpp::ResourceManager* resources, std::filesystem::path root)
	: Scene("Default", resources)
	, mRoot(std::move(root))
{
}

mpp::CameraPtr PackageScene::createCamera(ProgramOptions const& options) const
{
	return std::make_shared<mpp::helper::FpsCamera>(
		glm::vec3(0, 3, 8), 0.0f, 0.0f, 60.0f,
		float(options.screenWidth) / options.screenHeight);
}

void PackageScene::updateOrbitCamera()
{
	float cosine = std::cos(mOrbitPitch);
	glm::vec3 offset(
		std::sin(mOrbitYaw) * cosine,
		std::sin(mOrbitPitch),
		std::cos(mOrbitYaw) * cosine);
	getCamera()->setLookAt(mOrbitTarget + offset * mOrbitDistance, mOrbitTarget);
}

void PackageScene::setupImpl(mpp::RenderSystem* renderer, ProgramOptions const& options)
{
	mRenderer = renderer;
	auto manifest = mpp::app::readPackageManifest(mRoot / "manifest.xml");
	auto pipelineFile = mRoot / manifest.pipeline;
	auto sceneFile = mRoot / manifest.scene;
	if (!std::filesystem::is_regular_file(pipelineFile) || !std::filesystem::is_regular_file(sceneFile))
	{
		throw std::runtime_error("Package manifest refers to missing workspace documents.");
	}

	auto pipeline = std::make_shared<mpp::PbrPipelineDocument>(
		mpp::resource_parsers::PbrPipelineDocumentLoader::fromFile(pipelineFile.string()));
	mDocument = mpp::resource_parsers::SceneParser::fromFile(sceneFile.string());
	auto diagnostics = pipeline->validate(renderer->getCaps());
	diagnostics.append(pipeline->validateOutputAntiAliasing(renderer->getOptions().antiAliasing,&renderer->getCaps()));
	diagnostics.append(mDocument.validate());
	if (diagnostics.hasErrors())
	{
		throw std::runtime_error("Package pipeline or scene validation failed:\n" + diagnosticsSummary(diagnostics));
	}

	auto stream = std::make_shared<mpp::RenderGraphStream>(getResourceManager());
	stream->setGraph(pipeline->graph);
	auto graph = getResourceManager()->declareResource(mGraphResource, stream).first;
	graph->load();
	graph->create();

	mPipelineRuntime = std::make_unique<mpp::resource_parsers::PbrPipelineRuntime>(renderer, getResourceManager());
	if (!mPipelineRuntime->rebuild(pipeline, options.screenWidth, options.screenHeight))
	{
		throw std::runtime_error("Package pipeline runtime preparation failed:\n" + diagnosticsSummary(mPipelineRuntime->getDiagnostics()));
	}

	mpp::RenderPipelineOptions renderOptions;
	renderOptions.mode = mpp::RenderPipelineMode::XmlGraphPbrForward;
	renderOptions.graphTemplate = graph;
	renderOptions.graphImports = mPipelineRuntime->getImports();
	renderOptions.outputs = pipeline->outputs;
	mPresentationTarget = mPipelineRuntime->getPresentationTarget();
	if (!mPresentationTarget)
	{
		throw std::runtime_error("Package pipeline has no presentation target.");
	}
	renderOptions.environment = mPipelineRuntime->getEnvironment();
	renderOptions.bloom.enabled = pipeline->bloom.enabled;
	renderOptions.bloom.blurPasses = pipeline->bloom.blurPasses;
	// Lets MPP.FullscreenEffect passes resolve a programResource authored as a
	// bare PostEffectMaterial LocalResources name against this generation's
	// actual (dynamically-rooted) registered resource name.
	renderOptions.resourceRoot = mPipelineRuntime->getRootResource();

	if (auto direction = mDocument.getShadowLightDirection())
	{
		mpp::ShadowOptions shadow;
		shadow.enabled = true;
		shadow.light.direction = glm::normalize(*direction);
		shadow.light.focusPoint = mDocument.camera.target;
		renderOptions.shadowDomain = "DemoSuite.PackageShadow";
		renderer->configureShadowDomain(renderOptions.shadowDomain, shadow);
		renderOptions.graphImports["shadowDepth"] = renderer->getShadowDomainDepthTarget(renderOptions.shadowDomain);
	}
	auto packagePipeline=renderer->getOrCreateRenderPipeline("Package", renderOptions);
	std::map<std::string,mpp::RenderTargetPtr> outputDestinations;for(auto const& output:pipeline->outputs)for(uint32_t image=0;image<pipeline->graph->getImageCount();++image){auto info=pipeline->graph->getImageInfo({image,0});if(info.name!=output.image||!info.desc.external)continue;auto destination=renderOptions.graphImports.find(info.importName);if(destination!=renderOptions.graphImports.end())outputDestinations.emplace(output.name,destination->second);break;}if(outputDestinations.size()==pipeline->outputs.size())packagePipeline->prepareOutputs(*pipeline->graph,outputDestinations);

	mSceneRuntime = std::make_unique<mpp::SceneRuntime>(renderer, getResourceManager());
	if (!mSceneRuntime->rebuild(
		mDocument,
		mPipelineRuntime->getMaterialBindings(),
		mPipelineRuntime->getInstanceOverrides(),
		pipeline->environment.binding))
	{
		throw std::runtime_error("Package scene runtime preparation failed:\n" + diagnosticsSummary(mSceneRuntime->getDiagnostics()));
	}

	auto camera = getCamera();
	mOrbitTarget = mDocument.camera.target;
	auto offset = mDocument.camera.position - mOrbitTarget;
	mOrbitDistance = std::max(0.05f, glm::length(offset));
	mOrbitYaw = std::atan2(offset.x, offset.z);
	mOrbitPitch = std::asin(std::clamp(offset.y / mOrbitDistance, -1.0f, 1.0f));
	updateOrbitCamera();
	camera->markCut();
	camera->setFov(mDocument.camera.fov);
	camera->setClipDistances(mDocument.camera.nearPlane, mDocument.camera.farPlane);
	mPipelineRuntime->accept();
}

void PackageScene::teardownImpl()
{
	mSceneRuntime.reset();
	mPipelineRuntime.reset();
	mPresentationTarget.reset();
	if (mRenderer)
	{
		mRenderer->removeRenderPipeline("Package");
	}
	if (getResourceManager()->getResource(mGraphResource, true))
	{
		getResourceManager()->deleteResource(mGraphResource);
	}
	mRenderer = nullptr;
}

std::string PackageScene::getRenderPipelineName() const
{
	return "Package";
}

void PackageScene::handleInput(InputManager* input)
{
	float deltaX = 0.0f;
	float deltaY = 0.0f;
	input->getMouseDelta(&deltaX, &deltaY);

	bool alt = input->keyDown(Key_LeftAlt) || input->keyDown(Key_RightAlt);
	bool shift = input->keyDown(Key_LeftShift) || input->keyDown(Key_RightShift);
	bool control = input->keyDown(Key_LeftControl) || input->keyDown(Key_RightControl);
	bool middle = input->buttonDown(Mouse_Middle);
	bool trackpadDrag = alt && input->buttonDown(Mouse_Left);

	if ((middle && !shift && !control) || (trackpadDrag && !shift && !control))
	{
		mOrbitYaw -= deltaX * 0.01f;
		mOrbitPitch = std::clamp(mOrbitPitch - deltaY * 0.01f, -1.55f, 1.55f);
		updateOrbitCamera();
	}
	else if ((middle && shift) || (trackpadDrag && shift))
	{
		auto camera = getCamera();
		auto forward = glm::normalize(mOrbitTarget - camera->getPosition());
		auto right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
		auto up = glm::normalize(glm::cross(right, forward));
		mOrbitTarget += (-right * deltaX + up * deltaY) * (mOrbitDistance * 0.002f);
		updateOrbitCamera();
	}
	else if ((middle && control) || (trackpadDrag && control))
	{
		mOrbitDistance = std::clamp(mOrbitDistance * std::exp(deltaY * 0.01f), 0.05f, 100000.0f);
		updateOrbitCamera();
	}

	if (input->wheelUp() || input->wheelDown())
	{
		float factor = input->wheelUp() ? 0.85f : 1.0f / 0.85f;
		mOrbitDistance = std::clamp(mOrbitDistance * factor, 0.05f, 100000.0f);
		updateOrbitCamera();
	}
}

void PackageScene::render(mpp::RenderSystem* renderer, World const&, RenderOptions const&)
{
	if (!mSceneRuntime)
	{
		return;
	}

	renderer->renderScene(mSceneRuntime->getScene(), getCamera(), glm::vec2(0), "Package");
}

void PackageScene::present(mpp::RenderSystem* renderer)
{
	auto presentationTexture = std::dynamic_pointer_cast<mpp::RenderTexture>(mPresentationTarget);
	if (!presentationTexture)
	{
		throw std::runtime_error("Package presentation target is not a render texture.");
	}
	mpp::RenderSystem::TextureDiagnosticOptions presentationOptions;
	renderer->renderTextureDiagnostic(
		presentationTexture.get(),
		renderer->getScreenRenderTarget(),
		presentationOptions);
}
