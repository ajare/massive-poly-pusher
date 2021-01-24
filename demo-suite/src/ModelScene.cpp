/*

This scene displays various models:

- Programmatically-generated
- Built-in primitive shapes (eg cubes, planes, spheres)
- .mppmodel format

A Model is a Resource, and hence is created by a ResourceStream subclass.  This is either a
ProgrammaticModelStream, which lets you build the mesh(es) vertex-by-vertex, edge-by-edge,
polygon-by-polygon, or a PrimitiveModelStream which has subclasses which define the mesh
data required for the shape in question.  Finally, there is MppModelStream, which takes a
file in .mppmodel format, and loads that in.

An MppModel contains a basic Material definition, which includes shader information, and texture
information.  Shaders can either be files referenced relative to the .mppmodel's location, or
if empty, are generated using the default shader templates.  Textures are likewise either files
referenced relatively, or an existing resource which is expected to be loaded in.

The material is a child of the model, so is named "<model>/<material>", and the texture is
likewise a child of the material.  However, the program is not a child of the material, as it
is owned and shared by the ResourceManager and may be used by other meshes.

*/

#include <glm/gtx/rotate_vector.hpp>

#include <mpp/MppModelStream.h>
#include <mpp/SphereModelStream.h>
#include <mpp/GridModelStream.h>
#include <mpp/CylinderModelStream.h>
#include <mpp/BoxModelStream.h>
#include <mpp/ProgrammaticModelStream.h>
#include <mpp/ProgrammaticMaterialStream.h>
#include <mpp/ProgrammaticTextureStream.h>
#include <mpp/ProgrammaticSamplerStream.h>
#include <mpp/ResourceStreamSerializer.h>

#include <mpp/resource-parsers/FileTextureStream.h>
#include <mpp/resource-parsers/FileProgramStream.h>
#include <mpp/resource-parsers/FileMaterialStream.h>
#include <mpp/resource-parsers/FileStringStream.h>

#include <mpp/helper/FreeCamera.h>
#include <mpp/helper/OrbitCamera.h>
#include <mpp/helper/LineBatchRenderer.h>
#include <mpp/helper/TriangleBatchRenderer.h>

#include "ModelScene.h"
#include "Helper.h"
#include "TestLineBatchDataProvider.h"
#include "TestTriangleBatchDataProvider.h"

using namespace std;
using namespace mpp;

ModelScene::ModelScene(mpp::ResourceManager* resourceMgr)
	: Scene("Default", resourceMgr)
	, mLightPosition(0, 256, 256)
{
}

void ModelScene::createSharedTextures(ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	// Create a Sampler resource.  This holds parameters that shaders use when sampling textures.
	auto samplerStream = new ProgrammaticSamplerStream(resourceMgr);
	samplerStream->setFiltering(mpp::SamplerParams::MinFilter::Linear, mpp::SamplerParams::MagFilter::Linear);
	resourceMgr->declareResource("Default.Sampler", ResourceStreamPtr(samplerStream));

	// Create texture with sampler.
	auto textureStream = new ProgrammaticTextureStream(resourceMgr);
	textureStream->setTarget(TextureTarget::Texture2D);
	textureStream->setFile(options.resourceLocation + "marble_texture4662.jpg", loadImage);
	textureStream->enableMipMaps(true);
	textureStream->setSampler("Default.Sampler");
	resourceMgr->declareResource("Marble.Texture", ResourceStreamPtr(textureStream));

	// Create texture programmatically.  This is a 16bit texture.
	textureStream = new ProgrammaticTextureStream(resourceMgr);
	textureStream->setTarget(TextureTarget::Texture2D);
	textureStream->setFile(options.resourceLocation + "clouds_16.png", loadImage);
	textureStream->setFiltering(mpp::TextureParams::MinFilter::Linear, mpp::TextureParams::MagFilter::Linear);
	resourceMgr->declareResource("Clouds.Texture", ResourceStreamPtr(textureStream));

	textureStream = new ProgrammaticTextureStream(resourceMgr);
	textureStream->setTarget(TextureTarget::Texture2D);
	textureStream->setFile(options.resourceLocation + "electbubbles.jpg", loadImage);
	textureStream->setFiltering(mpp::TextureParams::MinFilter::LinearMipmapLinear, mpp::TextureParams::MagFilter::Linear);
	textureStream->enableMipMaps(true);
	resourceMgr->declareResource("Electro.Texture", ResourceStreamPtr(textureStream));

	textureStream = new ProgrammaticTextureStream(resourceMgr);
	textureStream->setTarget(TextureTarget::Texture2D);
	textureStream->setFile(options.resourceLocation + "test.png", loadImage);
	textureStream->setFiltering(mpp::TextureParams::MinFilter::Linear, mpp::TextureParams::MagFilter::Linear);
	resourceMgr->declareResource("Test.Texture", ResourceStreamPtr(textureStream));

	// Create texture from file definition.
	auto fileStream = new resource_parsers::FileTextureStream(resourceMgr, options.resourceLocation + "Doughnut.xml");
	resourceMgr->declareResource("Doughnut.Texture", ResourceStreamPtr(fileStream));

	// Create 1D texture from programmatic data.
	textureStream = new ProgrammaticTextureStream(resourceMgr);
	textureStream->setTarget(TextureTarget::Texture1D);
	textureStream->setData([](string const& id)
	{
		TextureData data;

		data.width = 256;
		data.height = 1;
		data.bitsPerPixel = 24;
		data.dataType = GL_UNSIGNED_BYTE;
		data.pixelFormat = GL_RGB;

		size_t dataSize = (data.width * data.height * data.bitsPerPixel / 8);
		
		data.data = new uint8_t[dataSize];
		for (int i = 0; i < 256; ++i)
		{
			data.data[i * 3 + 0] = 256 - i - 1;
			data.data[i * 3 + 1] = 0;
			data.data[i * 3 + 2] = i;
		}

		return data;
	});

	textureStream->setFiltering(mpp::TextureParams::MinFilter::Linear, mpp::TextureParams::MagFilter::Linear);
	resourceMgr->declareResource("Strip.Texture", ResourceStreamPtr(textureStream));
}

mesh::MeshSpecification ModelScene::createGridMeshSpecification()
{
	mesh::MeshSpecification meshSpec(mesh::Primitive::Type::Triangles);

	mesh::VertexBufferAttributeLayout* attribLayout = meshSpec.createVertexBufferAttributeLayout(false);
	attribLayout->createAttribute(mesh::Vertex::Component::Position3, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::Normal3, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::Float, true);

	meshSpec.setStorageType(mesh::VertexBufferStorageType::Static);
	meshSpec.setIndexedVertices(true);

	return meshSpec;
}

mpp::ResourcePtr ModelScene::createGridMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	// Create a String resource, which contains a vertex shader
	auto vertStream = new mpp::resource_parsers::FileStringStream(resourceMgr, options.resourceLocation + "Elevator.Vert.xml");
	auto vertRes = resourceMgr->declareResource("Elevator.Vert", ResourceStreamPtr(vertStream));
	vertRes->load();

	auto materialStream = new ProgrammaticMaterialStream(resourceMgr);
	materialStream->setProgram2d(false);
	materialStream->setMeshSpecification(meshSpec);

	// Use a vertex shader which raises the grid vertices up based on a texture, to create a heightmap effect.
	materialStream->setProgramVertexShaderResource("Elevator.Vert");
	materialStream->setTexture("TEX1", "Clouds.Texture");

	ResourceStreamPtr matStreamPtr(materialStream);

	auto res = resourceMgr->declareResource("Grid.Material", matStreamPtr);
	res->load();

	return res;
}

mesh::MeshSpecification ModelScene::createSphereMeshSpecification()
{
	mesh::MeshSpecification meshSpec(mesh::Primitive::Type::Triangles);

	mesh::VertexBufferAttributeLayout* attribLayout = meshSpec.createVertexBufferAttributeLayout(false);
	attribLayout->createAttribute(mesh::Vertex::Component::Position3, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::Normal3, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::UnsignedByte, true);

	meshSpec.setStorageType(mesh::VertexBufferStorageType::Static);
	meshSpec.setIndexedVertices(true);

	return meshSpec;
}

mpp::ResourcePtr ModelScene::createSphereMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	auto materialStream = new resource_parsers::FileMaterialStream(resourceMgr, options.resourceLocation + "ElectricMaterial.xml");
	auto res = resourceMgr->declareResource("Sphere.Material", ResourceStreamPtr(materialStream));
	res->load();

	return res;
}

mesh::MeshSpecification ModelScene::createCylinderMeshSpecification()
{
	mesh::MeshSpecification meshSpec(mesh::Primitive::Type::Triangles);

	mesh::VertexBufferAttributeLayout* attribLayout = meshSpec.createVertexBufferAttributeLayout(false);
	attribLayout->createAttribute(mesh::Vertex::Component::Position3, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::Normal3, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::Float, true);

	meshSpec.setStorageType(mesh::VertexBufferStorageType::Static);
	meshSpec.setIndexedVertices(true);

	return meshSpec;
}

mpp::ResourcePtr ModelScene::createCylinderMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	auto materialStream = new ProgrammaticMaterialStream(resourceMgr);
	materialStream->setProgram2d(false);
	materialStream->setMeshSpecification(meshSpec);
	materialStream->setTexture("TEX1", "Marble.Texture");

	auto res = resourceMgr->declareResource("Cylinder.Material", ResourceStreamPtr(materialStream));
	res->load();

	return res;
}

mesh::MeshSpecification ModelScene::createBoxMeshSpecification()
{
	mesh::MeshSpecification meshSpec(mesh::Primitive::Type::Triangles);

	mesh::VertexBufferAttributeLayout* attribLayout = meshSpec.createVertexBufferAttributeLayout(false);
	attribLayout->createAttribute(mesh::Vertex::Component::Position3, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::Normal3, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::Float, true);

	meshSpec.setStorageType(mesh::VertexBufferStorageType::Static);
	meshSpec.setIndexedVertices(true);

	return meshSpec;
}

mpp::ResourcePtr ModelScene::createBoxMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	auto materialStream = new ProgrammaticMaterialStream(resourceMgr);
	materialStream->setProgram2d(false);
	materialStream->setMeshSpecification(meshSpec);
	materialStream->setTexture("TEX1", "Test.Texture");

	auto res = resourceMgr->declareResource("Box.Material", ResourceStreamPtr(materialStream));
	res->load();

	return res;
}

mesh::MeshSpecification ModelScene::createTorusMeshSpecification()
{
	mesh::MeshSpecification meshSpec(mesh::Primitive::Type::Triangles);

	mesh::VertexBufferAttributeLayout* attribLayout = meshSpec.createVertexBufferAttributeLayout(false);
	attribLayout->createAttribute(mesh::Vertex::Component::Position3, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::Normal3, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::Float, true);

	meshSpec.setStorageType(mesh::VertexBufferStorageType::Static);
	meshSpec.setIndexedVertices(true);

	return meshSpec;
}

mpp::ResourcePtr ModelScene::createTorusMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	auto materialStream = new ProgrammaticMaterialStream(resourceMgr);
	materialStream->setProgram2d(false);
	materialStream->setMeshSpecification(meshSpec);
	materialStream->setTexture("TEX1", "Doughnut.Texture");

	auto res = resourceMgr->declareResource("Torus.Material", ResourceStreamPtr(materialStream));
	res->load();

	return res;
}

ResourcePtr ModelScene::createTorusModel(ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	auto torusMeshSpec = createTorusMeshSpecification();
	createTorusMaterial(torusMeshSpec, options);

	auto torusStream = new ProgrammaticModelStream(resourceMgr);
	auto torusMeshId = torusStream->createMesh("Torus", torusMeshSpec, "Torus.Material", 32);

	// Torus has 64 rings of 16 vertices each
	size_t ringSize{ 16 };
	size_t numRings{ 64 };
	size_t radius{ 48 };
	size_t thickness{ 12 };

	mesh::VertexData torusData(torusMeshSpec, ringSize * numRings);

	float dp = 2 * 3.14159f / ringSize;
	float dt = 2 * 3.14159f / numRings;

	for (size_t i = 0; i < numRings; ++i)
	{
		float theta = dt * i;

		for (size_t j = 0; j < ringSize; ++j)
		{
			float phi = dp * j;

			float nx = cosf(theta);
			float ny = sinf(phi);
			float nz = sinf(theta);

			float x = nx * (radius + cosf(phi) * thickness);
			float y = ny * thickness;
			float z = nz * (radius + cosf(phi) * thickness);
			
			// Hypertrochoid
			//float x = pow(cosf(theta), 3) * (radius + cosf(phi) * thickness);
			//float z = pow(sinf(theta), 3) * (radius + cosf(phi) * thickness);

			torusData.f32(x, y, z); // Position
			torusData.f32(nx, ny, nz); // Normal
			torusData.f32(i / ((float)numRings - 1) * 8, j / ((float)ringSize - 1)); // UV coord
			torusData.f32(1.0f, 1.0f, 1.0f, 1.0f); // Colour
		}
	}

	torusStream->addVertexData(torusMeshId, torusData);

	for (size_t i = 0; i < numRings; ++i)
	{
		for (size_t j = 0; j < ringSize; ++j)
		{
			auto i0 = i * ringSize + j;
			auto i1 = i * ringSize + ((j + 1) % ringSize);
			auto i2 = ((i + 1) % numRings) * ringSize + ((j + 1) % ringSize);
			auto i3 = ((i + 1) % numRings) * ringSize + j;

			torusStream->addTriangle(torusMeshId, i0, i1, i2);
			torusStream->addTriangle(torusMeshId, i2, i3, i0);
		}
	}

	auto torus = resourceMgr->declareResource("Model.Torus", ResourceStreamPtr(torusStream));
	torus->load();

	return torus;
}

void ModelScene::createBatches(mpp::RenderSystem* renderSystem)
{
	auto resourceMgr = getResourceManager();
	/*
	// Lines
	mpp::helper::LineBatchRendererParams lineParams
	{
		true,
		true,
		false
	};

	auto lineBatchDataProvider = make_shared<TestLineBatchDataProvider>();

	auto lineBatchRenderer = make_shared<mpp::helper::LineBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>>(
		"TestLines",
		lineParams,
		lineBatchDataProvider,
		renderSystem,
		resourceMgr);

	lineBatchRenderer->create();

	getScene()->add2dBatch(lineBatchDataProvider, lineBatchRenderer);
	*/
	// Triangles
	mpp::helper::TriangleBatchRendererParams triParams
	{
		false,
		true,
		true,
		false
	};
	
	auto triBatchDataProvider = make_shared<TestTriangleBatchDataProvider>();

	mTriangleBatch = make_shared<mpp::helper::TriangleBatch3DRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>>(
		"TestTris",
		triParams,
		triBatchDataProvider,
		resourceMgr->getResource("Electro.Texture"),
		renderSystem, resourceMgr);

	mTriangleBatch->create();

	mModels.push_back(getScene()->addModel(mTriangleBatch->getModel()));
	mModels.back()->translate(glm::vec3(0, 120, 120));
}

void ModelScene::setupImpl(mpp::RenderSystem* renderSystem, ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();
	auto mppScene = getScene();

	createSharedTextures(options);

	// Load Grid
	auto gridMeshSpec = createGridMeshSpecification();
	createGridMaterial(gridMeshSpec, options);

	auto gridStream = new GridModelStream(resourceMgr, gridMeshSpec, "Grid.Material", 1024, 1024, 256, 256);
	auto grid = resourceMgr->declareResource("Model.Grid", ResourceStreamPtr(gridStream));
	grid->load();

	mModels.push_back(mppScene->addModel(grid));

	// Load Sphere
	auto sphereMeshSpec = createSphereMeshSpecification();
	createSphereMaterial(sphereMeshSpec, options);
	
	auto sphereStream = new SphereModelStream(resourceMgr, sphereMeshSpec, "Sphere.Material", 40, 4);
	auto sphere = resourceMgr->declareResource("Model.Sphere", ResourceStreamPtr(sphereStream));
	sphere->load();
	
	mModels.push_back(mppScene->addModel(sphere));
	mModels.back()->translate(glm::vec3(-80, 130, 0));

	// Load Cylinder
	auto cylinderMeshSpec = createCylinderMeshSpecification();
	createCylinderMaterial(cylinderMeshSpec, options);

	auto cylinderStream = new CylinderModelStream(resourceMgr, cylinderMeshSpec, "Cylinder.Material", 80, 24, 24, 16);
	auto cylinder = resourceMgr->declareResource("Model.Cylinder", ResourceStreamPtr(cylinderStream));
	cylinder->load();

	mModels.push_back(mppScene->addModel(cylinder));
	mModels.back()->translate(glm::vec3(96, 40, 96));
	
	mModels.push_back(mppScene->addModel(cylinder));
	mModels.back()->translate(glm::vec3(-96, 40, 96));

	// Load Box
	auto boxMeshSpec = createBoxMeshSpecification();
	createBoxMaterial(boxMeshSpec, options);

	auto boxStream = new BoxModelStream(resourceMgr, cylinderMeshSpec, "Box.Material", 32, 32, 32);
	auto box = resourceMgr->declareResource("Model.Box", ResourceStreamPtr(boxStream));
	box->load();

	mModels.push_back(mppScene->addModel(box));
	mModels.back()->translate(glm::vec3(96, 108, 96));

	mModels.push_back(mppScene->addModel(box));
	mModels.back()->translate(glm::vec3(-96, 108, 96));

	// Load torus
	auto torus = createTorusModel(options);

	mModels.push_back(mppScene->addModel(torus));
	mModels.back()->translate(glm::vec3(0, 280, 0));
	
	// Load MppModel
	auto statueStream = new MppModelStream(resourceMgr, options.resourceLocation + "statue/statue.mppmodel");
	auto statue = resourceMgr->declareResource("Model.Statue", ResourceStreamPtr(statueStream));
	statue->load();

	mModels.push_back(mppScene->addModel(statue));

	// Lighting
	renderSystem->setAmbientColour(Colour::Grey25);
	renderSystem->setLightCount(1);
	renderSystem->setLight1Colour(Colour::White);

	// 2d batches
	createBatches(renderSystem);

	// Pipelines
	auto pipeline = renderSystem->createRenderPipeline(getRenderPipelineName());
}

mpp::CameraPtr ModelScene::createCamera(ProgramOptions const& options) const
{
	float aspectRatio = options.screenWidth / (float)options.screenHeight;

	auto camera = new helper::FreeCamera(glm::vec3(0, 150, 550), 0.0f, 0.0f, 0.0f, aspectRatio);

	camera->setClipDistances(0.1f, 1000.0f);
	camera->setFov(45.0f);

	return shared_ptr<mpp::Camera>(camera);
}

void ModelScene::update(mpp::RenderSystem* renderSystem, float frameTime)
{
	mTotalTime += frameTime;
	
	// Rotate sphere
	auto& sphereModel = mModels[1];

	float speed = 1.5f;
	sphereModel->rotateOrigin(-speed * frameTime, glm::vec3(0, 1, 0));

	// Rotate boxes
	auto a1 = glm::rotateX(glm::vec3(0, 1, 0), mTotalTime);
	auto a2 = glm::rotateZ(glm::vec3(0, 1, 0), mTotalTime);
	mModels[4]->rotateSelf(2 * frameTime, a1);
	mModels[5]->rotateSelf(-2 * frameTime, a2);

	// Rotate torus
	auto& torusModel = mModels[6];
	torusModel->rotateOrigin(speed * frameTime, glm::vec3(0, 1, 0));

	// Rotate batch box
	auto& batchBoxModel = mModels[8];
	batchBoxModel->rotateSelf(speed * frameTime, glm::normalize(glm::vec3(1, 1, 0)));
	
	// Lighting
	mLightPosition = glm::rotateY(mLightPosition, (2 * 3.14159f / 5.0f) * frameTime);
	mLightPosition.y = 128.0f + sinf(mTotalTime * 2.0f) * 128.0f;
	renderSystem->setLight1Position(mLightPosition);

	// Update scene
	getScene()->update(frameTime);
}

void ModelScene::render(mpp::RenderSystem* renderSystem, World const& world, RenderOptions const& options)
{
	renderSystem->renderScene(getScene(), getCamera(), "Default");
}