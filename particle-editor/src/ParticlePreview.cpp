#include "ParticlePreview.h"

#include <GL/glew.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec4.hpp>

#include <mpp/Camera.h>
#include <mpp/Colour.h>
#include <mpp/GridModelStream.h>
#include <mpp/ModelRenderParams.h>
#include <mpp/ParticleEffectValidator.h>
#include <mpp/PbrMaterialSpecification.h>
#include <mpp/PbrLight.h>
#include <mpp/ProgrammaticBasicMaterialStream.h>
#include <mpp/ProgrammaticParticleEffectStream.h>
#include <mpp/ProgrammaticPbrMaterialStream.h>
#include <mpp/ProgrammaticRenderTextureStream.h>
#include <mpp/ProgrammaticTextureStream.h>
#include <mpp/RenderGraphStream.h>
#include <mpp/RenderPipeline.h>
#include <mpp/RenderSystem.h>
#include <mpp/RenderTexture.h>
#include <mpp/ResourceManager.h>
#include <mpp/Scene.h>
#include <mpp/SceneModel3d.h>
#include <mpp/TextureData.h>
#include <mpp/resource-parsers/FileRenderGraphStream.h>
#include <mpp/mesh/MeshSpecification.h>
#include <mpp/mesh/Vertex.h>

namespace particle_editor
{
	namespace
	{
		constexpr char PbrPipeline[] = "ParticleEditor.PbrPreview";
		constexpr char LegacyPipeline[] = "ParticleEditor.LegacyPreview";
		constexpr char PbrGraph[] = "ParticleEditor.PbrGraph";
		constexpr char LegacyGraph[] = "ParticleEditor.LegacyGraph";
		constexpr char PreviewTarget[] = "ParticleEditor.Presentation";
		constexpr char StudioPlane[] = "ParticleEditor.StudioPlane";
		constexpr size_t GridLinesPerAxis = 11u;

		struct StudioColours
		{
			glm::vec3 face;
			glm::vec3 grid;
		};

		std::array<StudioColours, size_t(StudioPreset::Count)> const PresetColours{
			StudioColours{ { 0.43f, 0.46f, 0.5f }, { 0.11f, 0.13f, 0.16f } },
			StudioColours{ { 0.12f, 0.14f, 0.18f }, { 0.42f, 0.48f, 0.56f } },
			StudioColours{ { 0.5f, 0.38f, 0.27f }, { 0.16f, 0.1f, 0.07f } }
		};

		size_t graphIndex(PreviewGraph graph) { return graph == PreviewGraph::Pbr ? 0u : 1u; }
		char const* pipelineName(PreviewGraph graph) { return graph == PreviewGraph::Pbr ? PbrPipeline : LegacyPipeline; }
		char const* graphName(PreviewGraph graph) { return graph == PreviewGraph::Pbr ? PbrGraph : LegacyGraph; }

		mpp::mesh::MeshSpecification studioMeshSpecification()
		{
			mpp::mesh::MeshSpecification result(mpp::mesh::Primitive::Type::Triangles);
			auto* layout = result.createVertexBufferAttributeLayout(false);
			layout->createAttribute(mpp::mesh::Vertex::Component::Position3, mpp::mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mpp::mesh::Vertex::Component::Normal3, mpp::mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mpp::mesh::Vertex::Component::TexCoord2, mpp::mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mpp::mesh::Vertex::Component::Colour4, mpp::mesh::Vertex::DataType::Float, true);
			layout->createAttribute(mpp::mesh::Vertex::Component::Tangent4, mpp::mesh::Vertex::DataType::Float, false);
			result.setStorageType(mpp::mesh::VertexBufferStorageType::Static);
			result.setIndexedVertices(true);
			return result;
		}
	}

	ParticlePreview::ParticlePreview(mpp::RenderSystem* renderSystem, mpp::ResourceManager* resources)
		: mRenderSystem(renderSystem), mResources(resources)
	{
	}

	ParticlePreview::~ParticlePreview()
	{
		shutdown();
	}

	void ParticlePreview::setPreferencesPath(std::filesystem::path path)
	{
		mPreferencesPath = std::move(path);
		mPreferences = loadParticlePreviewPreferences(mPreferencesPath);
		if (mInitialised) applyPreferences();
	}

	void ParticlePreview::initialise(std::filesystem::path const& resourceRoot, uint32_t width, uint32_t height)
	{
		if (mInitialised) return;
		if (!mRenderSystem || !mResources) throw std::invalid_argument("Particle preview requires MPP services.");
		mWidth = std::max(64u, width);
		mHeight = std::max(64u, height);

		auto presentationStream = std::make_shared<mpp::ProgrammaticRenderTextureStream>(mResources);
		presentationStream->setTarget(mpp::TextureTarget::Texture2D);
		presentationStream->setInternalFormat(mpp::TextureInternalType::UnsignedInteger, true, 8u, 4u);
		presentationStream->setWidth(mWidth);
		presentationStream->setHeight(mHeight);
		presentationStream->setNumAttachments(1u);
		presentationStream->setFiltering(mpp::TextureParams::MinFilter::Linear, mpp::TextureParams::MagFilter::Linear);
		mPresentationResource = mResources->declareResource(PreviewTarget, presentationStream).first;
		mPresentationResource->load();
		mPresentationTexture = std::dynamic_pointer_cast<mpp::RenderTexture>(mPresentationResource);
		if (!mPresentationTexture) throw std::runtime_error("Particle preview presentation target is not a render texture.");

		for (auto const& [graph, file] : std::array{
			std::pair{ PreviewGraph::Pbr, "PbrPreview.rendergraph.yaml" },
			std::pair{ PreviewGraph::Legacy, "LegacyPreview.rendergraph.yaml" } })
		{
			auto graphPath = resourceRoot / "particle-editor" / file;
			if (!std::filesystem::is_regular_file(graphPath))
				throw std::runtime_error("Particle Editor preview graph was not found: " + graphPath.string());
			auto stream = std::make_shared<mpp::resource_parsers::FileRenderGraphStream>(mResources, graphPath.string());
			mGraphResources[graphIndex(graph)] = mResources->declareResource(graphName(graph), stream).first;
		}

		mScene = std::make_shared<mpp::Scene>(mRenderSystem);
		mScene->load();
		mScene->setClearColour(mpp::Colour(0.025f, 0.035f, 0.055f));
		mScene->setViewport(0, 0, mWidth, mHeight);
		mCamera = std::make_shared<mpp::Camera>(glm::vec3(0.0f, 2.6f, 7.0f), 0.0f, 0.0f, 0.0f,
			55.0f, float(mWidth) / float(mHeight));
		mCamera->setClipDistances(0.05f, 1000.0f);

		createStudioResources();
		auto& particles = mRenderSystem->getParticleSystem();
		particles.initialise();
		particles.setStatisticsEnabled(true);
		mInitialised = true;
		updateStudioGeometry();
		updateCamera();
		updateLight();

		std::string preferredFailure;
		auto const preferred = mPreferences.graph;
		if (!selectGraph(preferred, &preferredFailure))
		{
			std::string fallbackFailure;
			auto const fallback = preferred == PreviewGraph::Pbr ? PreviewGraph::Legacy : PreviewGraph::Pbr;
			if (!selectGraph(fallback, &fallbackFailure))
				throw std::runtime_error(preferredFailure + " " + fallbackFailure);
			mGraphFailure = preferredFailure + " The other graph remains active.";
		}
		mPreferencesDirty = false;
	}

	void ParticlePreview::createStudioResources()
	{
		auto const mesh = studioMeshSpecification();
		auto declareOwned = [&](std::string const& name, mpp::ResourceStreamPtr const& stream)
		{
			auto [resource, inserted] = mResources->declareResource(name, stream);
			if (!inserted) throw std::runtime_error("Particle preview resource already exists: " + name);
			mOwnedResourceNames.push_back(name);
			resource->load();
			return resource;
		};
		auto makeTexture = [&](std::string const& name, glm::vec3 colour)
		{
			auto stream = std::make_shared<mpp::ProgrammaticTextureStream>(mResources);
			stream->setTarget(mpp::TextureTarget::Texture2D);
			stream->setInternalFormat(mpp::TextureInternalType::UnsignedInteger, true, 8u, 4u);
			stream->setColourSpace(mpp::TextureColourSpace::Srgb);
			stream->setFiltering(mpp::TextureParams::MinFilter::Nearest, mpp::TextureParams::MagFilter::Nearest);
			stream->setData([colour](std::string const&)
			{
				mpp::TextureData data;
				data.width = data.height = 1;
				data.bitsPerPixel = 32;
				data.dataType = GL_UNSIGNED_BYTE;
				data.pixelFormat = GL_RGBA;
				data.data = new uint8_t[4]{
					uint8_t(glm::clamp(colour.r, 0.0f, 1.0f) * 255.0f),
					uint8_t(glm::clamp(colour.g, 0.0f, 1.0f) * 255.0f),
					uint8_t(glm::clamp(colour.b, 0.0f, 1.0f) * 255.0f), 255u };
				return data;
			});
			return declareOwned(name, stream);
		};

		for (size_t preset = 0; preset < PresetColours.size(); ++preset)
		{
			auto const suffix = std::to_string(preset);
			auto faceTexture = makeTexture("ParticleEditor.StudioFaceTexture." + suffix, PresetColours[preset].face);
			auto gridTexture = makeTexture("ParticleEditor.StudioGridTexture." + suffix, PresetColours[preset].grid);
			auto legacyFace = std::make_shared<mpp::ProgrammaticBasicMaterialStream>(mResources);
			legacyFace->setMeshSpecification(mesh);
			legacyFace->setProgram2d(false);
			legacyFace->setTexture("TEX1", faceTexture->getName());
			mLegacyStudioMaterials[preset] = declareOwned("ParticleEditor.LegacyStudioMaterial." + suffix, legacyFace);
			auto legacyGrid = std::make_shared<mpp::ProgrammaticBasicMaterialStream>(mResources);
			legacyGrid->setMeshSpecification(mesh);
			legacyGrid->setProgram2d(false);
			legacyGrid->setTexture("TEX1", gridTexture->getName());
			mLegacyGridMaterials[preset] = declareOwned("ParticleEditor.LegacyGridMaterial." + suffix, legacyGrid);

			auto pbrSurface = mpp::PbrMaterialSpecification::PbrSurface{};
			pbrSurface.enabled = true;
			pbrSurface.baseColourFactor = glm::vec4(PresetColours[preset].face, 1.0f);
			pbrSurface.metallicFactor = 0.0f;
			pbrSurface.roughnessFactor = 0.82f;
			auto pbrFace = std::make_shared<mpp::ProgrammaticPbrMaterialStream>(mResources);
			pbrFace->setMeshSpecification(mesh);
			pbrFace->setProgram2d(false);
			pbrFace->setSurface(pbrSurface);
			mPbrStudioMaterials[preset] = declareOwned("ParticleEditor.PbrStudioMaterial." + suffix, pbrFace);
			pbrSurface.baseColourFactor = glm::vec4(PresetColours[preset].grid, 1.0f);
			auto pbrGrid = std::make_shared<mpp::ProgrammaticPbrMaterialStream>(mResources);
			pbrGrid->setMeshSpecification(mesh);
			pbrGrid->setProgram2d(false);
			pbrGrid->setSurface(pbrSurface);
			mPbrGridMaterials[preset] = declareOwned("ParticleEditor.PbrGridMaterial." + suffix, pbrGrid);
		}

		auto planeStream = std::make_shared<mpp::GridModelStream>(mResources, mesh,
			mPbrStudioMaterials[0]->getName(), 1.0, 1.0, 1u, 1u);
		mStudioPlaneResource = declareOwned(StudioPlane, planeStream);
		for (auto& face : mStudioFaces)
		{
			face = mScene->add3dModel(mStudioPlaneResource);
			face->getParams()->setModelFlags(mpp::ModelRenderParams::Flag_Visible |
				mpp::ModelRenderParams::Flag_CullBackFaces);
			face->getParams()->setModelDepthPrepass(true);
			face->getParams()->setModelBlend(false);
		}
		mFloorGrid.reserve(GridLinesPerAxis * 2u);
		for (size_t line = 0; line < GridLinesPerAxis * 2u; ++line)
		{
			auto instance = mScene->add3dModel(mStudioPlaneResource);
			instance->getParams()->setModelFlags(mpp::ModelRenderParams::Flag_Visible |
				mpp::ModelRenderParams::Flag_CullBackFaces);
			instance->getParams()->setModelDepthPrepass(true);
			instance->getParams()->setModelBlend(false);
			mFloorGrid.push_back(std::move(instance));
		}
	}

	void ParticlePreview::updateStudioGeometry()
	{
		if (!mScene) return;
		auto const center = mStudio.center;
		auto const half = mStudio.size * 0.5f;
		auto place = [](std::shared_ptr<mpp::SceneModel3d> const& model, glm::vec3 translation,
			float rotation, glm::vec3 axis, glm::vec3 scale)
		{
			model->resetTransform();
			model->translate(translation);
			if (rotation != 0.0f) model->rotateSelf(rotation, axis);
			model->scale(scale);
		};
		place(mStudioFaces[0], center + glm::vec3(0.0f, -half.y, 0.0f), 0.0f, { 1, 0, 0 }, { mStudio.size.x, 1, mStudio.size.z });
		place(mStudioFaces[1], center + glm::vec3(0.0f, half.y, 0.0f), glm::pi<float>(), { 1, 0, 0 }, { mStudio.size.x, 1, mStudio.size.z });
		place(mStudioFaces[2], center + glm::vec3(-half.x, 0.0f, 0.0f), -glm::half_pi<float>(), { 0, 0, 1 }, { mStudio.size.y, 1, mStudio.size.z });
		place(mStudioFaces[3], center + glm::vec3(half.x, 0.0f, 0.0f), glm::half_pi<float>(), { 0, 0, 1 }, { mStudio.size.y, 1, mStudio.size.z });
		place(mStudioFaces[4], center + glm::vec3(0.0f, 0.0f, -half.z), glm::half_pi<float>(), { 1, 0, 0 }, { mStudio.size.x, 1, mStudio.size.y });
		place(mStudioFaces[5], center + glm::vec3(0.0f, 0.0f, half.z), -glm::half_pi<float>(), { 1, 0, 0 }, { mStudio.size.x, 1, mStudio.size.y });

		float const thickness = std::max(0.005f, std::min(mStudio.size.x, mStudio.size.z) * 0.004f);
		float const floor = center.y - half.y + std::max(0.001f, mStudio.size.y * 0.0005f);
		for (size_t line = 0; line < GridLinesPerAxis; ++line)
		{
			float const t = float(line) / float(GridLinesPerAxis - 1u);
			place(mFloorGrid[line], { center.x, floor, center.z - half.z + t * mStudio.size.z },
				0.0f, { 1, 0, 0 }, { mStudio.size.x, 1, thickness });
			place(mFloorGrid[GridLinesPerAxis + line], { center.x - half.x + t * mStudio.size.x, floor, center.z },
				0.0f, { 1, 0, 0 }, { thickness, 1, mStudio.size.z });
		}
		updateStudioVisibility();
		updateLight();
	}

	void ParticlePreview::updateStudioVisibility()
	{
		if (!mCamera) return;
		auto const hidden = obstructingStudioFaces(mStudio, mCamera->getPosition());
		constexpr std::array<uint32_t, 6> bits{
			StudioFloor, StudioCeiling, StudioLeft, StudioRight, StudioBack, StudioFront };
		for (size_t face = 0; face < mStudioFaces.size(); ++face)
		{
			auto flags = mpp::ModelRenderParams::Flag_CullBackFaces;
			if ((hidden & bits[face]) == 0u) flags |= mpp::ModelRenderParams::Flag_Visible;
			mStudioFaces[face]->getParams()->setModelFlags(flags);
		}
		auto gridFlags = mpp::ModelRenderParams::Flag_CullBackFaces;
		if (mPreferences.floorGrid && (hidden & StudioFloor) == 0u) gridFlags |= mpp::ModelRenderParams::Flag_Visible;
		for (auto const& line : mFloorGrid) line->getParams()->setModelFlags(gridFlags);
	}

	void ParticlePreview::applyStudioMaterials()
	{
		if (!mScene) return;
		auto preset = std::min(size_t(mPreferences.studioPreset), size_t(StudioPreset::Count) - 1u);
		auto const& face = mActiveGraph == PreviewGraph::Pbr ? mPbrStudioMaterials[preset] : mLegacyStudioMaterials[preset];
		auto const& grid = mActiveGraph == PreviewGraph::Pbr ? mPbrGridMaterials[preset] : mLegacyGridMaterials[preset];
		for (auto const& instance : mStudioFaces) instance->getParams()->setModelMaterial(face);
		for (auto const& instance : mFloorGrid) instance->getParams()->setModelMaterial(grid);
	}

	void ParticlePreview::updateCamera()
	{
		if (!mCamera) return;
		float const cosine = std::cos(mPreferences.cameraPitch);
		glm::vec3 const offset(std::sin(mPreferences.cameraYaw) * cosine,
			std::sin(mPreferences.cameraPitch), std::cos(mPreferences.cameraYaw) * cosine);
		mCamera->setLookAt(mPreferences.cameraTarget + offset * mPreferences.cameraDistance,
			mPreferences.cameraTarget);
		updateStudioVisibility();
	}

	void ParticlePreview::updateLight()
	{
		if (!mRenderSystem || !mScene) return;
		mRenderSystem->setAmbientColour(mpp::Colour(0.34f, 0.34f, 0.36f));
		mRenderSystem->setPbrAmbientColour(mpp::Colour(0.12f, 0.12f, 0.13f));
		if (!mPreferences.lightEnabled)
		{
			mRenderSystem->setLightCount(0u);
			mScene->setPbrLights({});
			return;
		}
		auto const position = previewLightPosition(mPreferences, mStudio);
		mRenderSystem->setLightCount(1u);
		mRenderSystem->setLight1Position(position);
		auto const legacy = mPreferences.lightColour * (0.35f * mPreferences.lightIntensity);
		mRenderSystem->setLight1Colour(mpp::Colour(legacy.r, legacy.g, legacy.b));
		mpp::PbrLight light;
		light.type = mpp::PbrLightType::Point;
		light.position = position;
		light.colour = mPreferences.lightColour;
		light.intensity = mPreferences.lightIntensity;
		light.range = std::max(mPreferences.lightDistance * 3.0f, glm::length(mStudio.size) * 1.5f);
		mScene->setPbrLights({ light });
	}

	void ParticlePreview::setEffectBounds(std::optional<mpp::ParticleEffectBounds> bounds)
	{
		mEffectBounds = std::move(bounds);
		mStudio = studioVolumeForBounds(mEffectBounds);
		updateStudioGeometry();
	}

	bool ParticlePreview::ensureGraphInstalled(PreviewGraph graph, std::string* failure)
	{
		auto const index = graphIndex(graph);
		if (mInstalledGraphs[index]) return true;
		try
		{
			mGraphResources[index]->load();
			mpp::RenderPipelineOptions options;
			options.mode = graph == PreviewGraph::Pbr ? mpp::RenderPipelineMode::XmlGraphPbrForward :
				mpp::RenderPipelineMode::GraphLegacyForward;
			options.graphTemplate = mGraphResources[index];
			options.graphImports["screen"] = mPresentationTexture;
			mRenderSystem->getOrCreateRenderPipeline(pipelineName(graph), options);
			mInstalledGraphs[index] = true;
			return true;
		}
		catch (std::exception const& error)
		{
			try { mRenderSystem->removeRenderPipeline(pipelineName(graph)); } catch (...) {}
			if (failure) *failure = std::string(graph == PreviewGraph::Pbr ? "PBR" : "Legacy") +
				" preview graph installation failed: " + error.what();
			return false;
		}
	}

	bool ParticlePreview::selectGraph(PreviewGraph graph, std::string* failure)
	{
		if (!mInitialised)
		{
			if (failure) *failure = "Particle preview has not been initialised.";
			return false;
		}
		std::string installationFailure;
		if (!ensureGraphInstalled(graph, &installationFailure))
		{
			mGraphFailure = installationFailure;
			if (failure) *failure = installationFailure;
			return false;
		}
		mActiveGraph = graph;
		mActivePipeline = pipelineName(graph);
		mPreferences.graph = graph;
		applyStudioMaterials();
		mCamera->markCut();
		mGraphFailure.clear();
		mPreferencesDirty = true;
		if (failure) failure->clear();
		return true;
	}

	bool ParticlePreview::install(mpp::ParticleEffectSpecification const& specification, std::string* failure)
	{
		if (!mInitialised)
		{
			if (failure) *failure = "Particle preview has not been initialised.";
			return false;
		}
		auto diagnostics = mpp::ParticleEffectValidator::validate(specification);
		if (diagnostics.hasErrors())
		{
			if (failure)
			{
				*failure = "The particle effect is invalid.";
				for (auto const& diagnostic : diagnostics.getDiagnostics())
					if (diagnostic.severity == mpp::DiagnosticSeverity::Error)
					{
						*failure += " [" + diagnostic.code + "] " + diagnostic.message;
						break;
					}
			}
			return false;
		}

		auto candidateName = "ParticleEditor.Effect." + std::to_string(++mEffectGeneration);
		mpp::ResourcePtr candidate;
		mpp::ParticleEffectHandle candidateEffect;
		try
		{
			auto stream = std::make_shared<mpp::ProgrammaticParticleEffectStream>(mResources);
			stream->setSpecification(specification);
			candidate = mResources->declareResource(candidateName, stream).first;
			candidate->load();
			candidateEffect = mRenderSystem->getParticleSystem().createEffect(candidate);

			auto oldResource = mEffectResource;
			auto oldEffect = mEffect;
			mEffectResource = std::move(candidate);
			mEffect = candidateEffect;
			setEffectBounds(specification.bounds);
			if (oldEffect && mRenderSystem->getParticleSystem().isAlive(oldEffect))
				mRenderSystem->getParticleSystem().destroyEffect(oldEffect);
			if (oldResource)
			{
				auto oldName = oldResource->getName();
				oldResource.reset();
				mResources->deleteResourceTree(oldName);
			}
			++mRebuildCount;
			if (failure) failure->clear();
			return true;
		}
		catch (std::exception const& error)
		{
			if (candidateEffect && mRenderSystem->getParticleSystem().isAlive(candidateEffect))
				mRenderSystem->getParticleSystem().destroyEffect(candidateEffect);
			if (candidate)
			{
				auto name = candidate->getName();
				candidate.reset();
				try { mResources->deleteResourceTree(name); } catch (...) {}
			}
			if (failure) *failure = error.what();
			return false;
		}
	}

	bool ParticlePreview::updateLive(mpp::ParticleEffectSpecification const& specification, std::string* failure)
	{
		if (!ready())
		{
			if (failure) *failure = "The live particle preview is not available.";
			return false;
		}
		auto diagnostics = mpp::ParticleEffectValidator::validate(specification);
		if (diagnostics.hasErrors())
		{
			if (failure) *failure = "The particle effect is invalid.";
			return false;
		}

		auto& particles = mRenderSystem->getParticleSystem();
		for (size_t index = 0; index < specification.emitterTemplates.size(); ++index)
			if (!particles.getEmitter(mEffect, index))
			{
				if (failure) *failure = "The live emitter-template structure no longer matches the document.";
				return false;
			}
		for (size_t index = 0; index < specification.emitterTemplates.size(); ++index)
		{
			auto const& emitter = specification.emitterTemplates[index].value;
			particles.updateEmitterTemplateRuntime(particles.getEmitter(mEffect, index),
				emitter.simulation, emitter.appearance);
		}
		setEffectBounds(specification.bounds);
		++mLiveUpdateCount;
		if (failure) failure->clear();
		return true;
	}

	void ParticlePreview::resize(uint32_t width, uint32_t height)
	{
		if (!mInitialised || width < 64u || height < 64u || (width == mWidth && height == mHeight)) return;
		mPresentationTexture->resize(width, height);
		for (size_t graph = 0; graph < mInstalledGraphs.size(); ++graph)
			if (mInstalledGraphs[graph]) mRenderSystem->getRenderPipeline(graph == 0 ? PbrPipeline : LegacyPipeline)->resize(width, height);
		mScene->setViewport(0, 0, width, height);
		mCamera->setAspectRatio(float(width) / float(height));
		mCamera->markCut();
		mWidth = width;
		mHeight = height;
	}

	void ParticlePreview::pauseSimulation() { mRenderSystem->getParticleSystem().pauseSimulation(); }
	void ParticlePreview::resumeSimulation() { mRenderSystem->getParticleSystem().resumeSimulation(); }
	void ParticlePreview::stepSimulation(float deltaSeconds) { mRenderSystem->getParticleSystem().requestSimulationStep(deltaSeconds); }
	void ParticlePreview::setSimulationTimeScale(float scale) { mRenderSystem->getParticleSystem().setSimulationTimeScale(scale); }
	bool ParticlePreview::isSimulationPaused() const { return mRenderSystem->getParticleSystem().isSimulationPaused(); }
	float ParticlePreview::simulationTimeScale() const { return mRenderSystem->getParticleSystem().getSimulationTimeScale(); }

	void ParticlePreview::orbitCamera(float horizontal, float vertical)
	{
		mPreferences.cameraYaw += horizontal;
		mPreferences.cameraPitch = std::clamp(mPreferences.cameraPitch + vertical, -1.5f, 1.5f);
		updateCamera();
		mPreferencesDirty = true;
	}

	void ParticlePreview::panCamera(float horizontal, float vertical)
	{
		auto right = glm::normalize(glm::cross(mCamera->getDirection(), mCamera->getUp()));
		float const scale = mPreferences.cameraDistance * 0.002f;
		mPreferences.cameraTarget += right * horizontal * scale + mCamera->getUp() * vertical * scale;
		updateCamera();
		mPreferencesDirty = true;
	}

	void ParticlePreview::zoomCamera(float amount)
	{
		mPreferences.cameraDistance = std::clamp(mPreferences.cameraDistance * std::exp(amount), 0.05f, 100000.0f);
		updateCamera();
		mPreferencesDirty = true;
	}

	void ParticlePreview::focusSelection(mpp::ParticleEffectSpecification const& specification,
		std::optional<size_t> emitterIndex)
	{
		if (emitterIndex && *emitterIndex < specification.emitterTemplates.size())
		{
			auto const& transform = specification.emitterTemplates[*emitterIndex].value.simulation.transform;
			mPreferences.cameraTarget = { transform[12], transform[13], transform[14] };
			mPreferences.cameraDistance = std::max(2.0f, glm::length(mStudio.size) * 0.22f);
			updateCamera();
			mPreferencesDirty = true;
			return;
		}
		frameBounds();
	}

	void ParticlePreview::frameBounds()
	{
		mPreferences.cameraTarget = mEffectBounds ? mEffectBounds->center : mStudio.center;
		float const radius = glm::length((mEffectBounds ? mEffectBounds->size : mStudio.size) * 0.5f);
		float const halfFov = glm::radians(mCamera ? mCamera->getFov() : 55.0f) * 0.5f;
		mPreferences.cameraDistance = std::max(0.5f, radius / std::max(std::tan(halfFov), 0.05f) * 1.15f);
		updateCamera();
		mPreferencesDirty = true;
	}

	void ParticlePreview::resetCamera()
	{
		mPreferences.cameraYaw = 0.0f;
		mPreferences.cameraPitch = 0.28f;
		frameBounds();
		mCamera->markCut();
	}

	void ParticlePreview::manipulateLight(float horizontal, float vertical)
	{
		mPreferences.lightAzimuth += horizontal;
		mPreferences.lightElevation = std::clamp(mPreferences.lightElevation + vertical, -1.5f, 1.5f);
		updateLight();
		mPreferencesDirty = true;
	}

	std::optional<glm::vec2> ParticlePreview::lightViewportPosition() const
	{
		if (!mCamera || !mPreferences.lightEnabled || mWidth == 0u || mHeight == 0u) return std::nullopt;
		auto const world = previewLightPosition(mPreferences, mStudio);
		auto const clip = mCamera->getProjectionTransform() * mCamera->getViewTransform() * glm::vec4(world, 1.0f);
		if (clip.w <= 0.0f) return std::nullopt;
		auto const ndc = glm::vec3(clip) / clip.w;
		if (std::abs(ndc.x) > 1.1f || std::abs(ndc.y) > 1.1f || ndc.z < -1.0f || ndc.z > 1.0f) return std::nullopt;
		return glm::vec2((ndc.x * 0.5f + 0.5f) * float(mWidth),
			(0.5f - ndc.y * 0.5f) * float(mHeight));
	}

	void ParticlePreview::applyPreferences()
	{
		applyStudioMaterials();
		updateCamera();
		updateStudioVisibility();
		updateLight();
		mPreferencesDirty = true;
	}

	void ParticlePreview::savePreferences()
	{
		if (!mPreferencesDirty || mPreferencesPath.empty()) return;
		saveParticlePreviewPreferences(mPreferencesPath, mPreferences);
		mPreferencesDirty = false;
	}

	void ParticlePreview::update(float deltaSeconds)
	{
		if (mPreferences.lightEnabled && mPreferences.lightAutoOrbit && deltaSeconds > 0.0f)
		{
			mPreferences.lightAzimuth += mPreferences.lightAutoOrbitSpeed * deltaSeconds;
			updateLight();
			mPreferencesDirty = true;
		}
	}

	void ParticlePreview::render()
	{
		if (ready() && !mActivePipeline.empty())
		{
			updateStudioVisibility();
			mRenderSystem->renderScene(mScene, mCamera, glm::vec2(0.0f), mActivePipeline);
		}
	}

	mpp::ParticleStats const& ParticlePreview::stats() const
	{
		return mRenderSystem->getParticleSystem().getStats();
	}

	bool ParticlePreview::ready() const
	{
		return mInitialised && !mActivePipeline.empty() && bool(mEffectResource) &&
			mRenderSystem->getParticleSystem().isAlive(mEffect);
	}

	void ParticlePreview::shutdown() noexcept
	{
		if (!mInitialised) return;
		try { savePreferences(); } catch (...) {}
		try
		{
			auto& particles = mRenderSystem->getParticleSystem();
			if (mEffect && particles.isAlive(mEffect)) particles.destroyEffect(mEffect);
			particles.setStatisticsEnabled(false);
			mEffect = {};
			if (mEffectResource)
			{
				auto name = mEffectResource->getName();
				mEffectResource.reset();
				mResources->deleteResourceTree(name);
			}
			if (mScene) mScene->unload();
			mFloorGrid.clear();
			mStudioFaces = {};
			mScene.reset();
			mCamera.reset();
			for (size_t graph = 0; graph < mInstalledGraphs.size(); ++graph)
				if (mInstalledGraphs[graph]) mRenderSystem->removeRenderPipeline(graph == 0 ? PbrPipeline : LegacyPipeline);
			mGraphResources = {};
			for (auto const* name : { PbrGraph, LegacyGraph })
				if (mResources->getResource(name, true)) mResources->deleteResourceTree(name);
			mStudioPlaneResource.reset();
			mPbrStudioMaterials = {};
			mLegacyStudioMaterials = {};
			mPbrGridMaterials = {};
			mLegacyGridMaterials = {};
			for (auto name = mOwnedResourceNames.rbegin(); name != mOwnedResourceNames.rend(); ++name)
				if (mResources->getResource(*name, true)) mResources->deleteResourceTree(*name);
			mOwnedResourceNames.clear();
			mPresentationTexture.reset();
			mPresentationResource.reset();
			if (mResources->getResource(PreviewTarget, true)) mResources->deleteResourceTree(PreviewTarget);
		}
		catch (...) {}
		mInstalledGraphs = {};
		mActivePipeline.clear();
		mInitialised = false;
	}
}
