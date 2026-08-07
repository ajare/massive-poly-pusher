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
#include <filesystem>
#include <fstream>

#include <stdexcept>

#include <mpp/MppModelStream.h>
#include <mpp/SphereModelStream.h>
#include <mpp/GridModelStream.h>
#include <mpp/CylinderModelStream.h>
#include <mpp/BoxModelStream.h>
#include <mpp/ProgrammaticModelStream.h>
#include <mpp/ProgrammaticBasicMaterialStream.h>
#include <mpp/ProgrammaticPbrMaterialStream.h>
#include <mpp/ProgrammaticStringStream.h>
#include <mpp/PbrMaterial.h>
#include <mpp/PbrShaders.h>
#include <mpp/ProgrammaticTextureStream.h>
#include <mpp/ProgrammaticTextureStream.h>
#include <mpp/ProgrammaticSamplerStream.h>
#include <mpp/ResourceStreamSerializer.h>
#include <mpp/RenderGraphGpuTests.h>
#include <mpp/PbrMaterialTests.h>
#include <mpp/DiagnosticTests.h>
#include <mpp/app/DocumentFoundationTests.h>

#include <mpp/resource-parsers/FileTextureStream.h>
#include <mpp/resource-parsers/FileProgramStream.h>
#include <mpp/resource-parsers/FileMaterialStream.h>
#include <mpp/resource-parsers/MaterialResourceTests.h>
#include <mpp/resource-parsers/FileStringStream.h>
#include <mpp/resource-parsers/FileRenderGraphStream.h>
#include <mpp/resource-parsers/PbrPipelineParser.h>
#include <mpp/resource-parsers/PbrPipelineDocumentLoader.h>
#include <mpp/resource-parsers/PbrPipelineSerializer.h>
#include <mpp/resource-parsers/FilePbrPipelineStream.h>
#include <mpp/resource-parsers/SceneParser.h>
#include <mpp/resource-parsers/SceneSerializer.h>
#include <mpp/resource-parsers/FileSceneStream.h>

#include <mpp/helper/FreeCamera.h>
#include <mpp/helper/FpsCamera.h>
#include <mpp/helper/LineBatchRenderer.h>
#include <mpp/helper/TriangleBatchRenderer.h>
#include <mpp/helper/QuadBatchRenderer.h>

#include "imgui/imgui.h"

#include "ModelScene.h"
#include "Helper.h"
#include "TestLineBatchDataProvider.h"
#include "Test2dTriangleBatchDataProvider.h"
#include "Test3dTriangleBatchDataProvider.h"
#include "TestQuadBatchDataProvider.h"

using namespace std;
using namespace mpp;

ModelScene::ModelScene(mpp::ResourceManager* resourceMgr)
	: Scene("Default", resourceMgr)
	, mLightPosition(0, 256, 256)
	, mImGuiRenderer(nullptr)
{
}

void ModelScene::createSharedTextures(ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	// Create a Sampler resource.  This holds parameters that shaders use when sampling textures.
	auto samplerStream = new ProgrammaticSamplerStream(resourceMgr);
	samplerStream->setFiltering(mpp::SamplerParams::MinFilter::Linear, mpp::SamplerParams::MagFilter::Linear);
	addResource(resourceMgr->declareResource("Default.Sampler", ResourceStreamPtr(samplerStream)).first, false);

	// Create texture with sampler.
	auto textureStream = new ProgrammaticTextureStream(resourceMgr);
	textureStream->setTarget(TextureTarget::Texture2D);
	textureStream->setFile(options.resourceLocation + "marble_texture4662.jpg", loadImage);
	textureStream->setColourSpace(mpp::TextureColourSpace::Srgb);
	textureStream->enableMipMaps(true);
	textureStream->setSampler("Default.Sampler");
	addResource(resourceMgr->declareResource("Marble.Texture", ResourceStreamPtr(textureStream)).first, false);

	// Create texture programmatically.  This is a 16bit texture.
	textureStream = new ProgrammaticTextureStream(resourceMgr);
	textureStream->setTarget(TextureTarget::Texture2D);
	textureStream->setFile(options.resourceLocation + "clouds_16.png", loadImage);
	textureStream->setFiltering(mpp::TextureParams::MinFilter::Linear, mpp::TextureParams::MagFilter::Linear);
	addResource(resourceMgr->declareResource("Clouds.Texture", ResourceStreamPtr(textureStream)).first, false);

	textureStream = new ProgrammaticTextureStream(resourceMgr);
	textureStream->setTarget(TextureTarget::Texture2D);
	textureStream->setFile(options.resourceLocation + "electbubbles.jpg", loadImage);
	textureStream->setFiltering(mpp::TextureParams::MinFilter::LinearMipmapLinear, mpp::TextureParams::MagFilter::Linear);
	textureStream->enableMipMaps(true);
	addResource(resourceMgr->declareResource("Electro.Texture", ResourceStreamPtr(textureStream)).first, false);

	textureStream = new ProgrammaticTextureStream(resourceMgr);
	textureStream->setTarget(TextureTarget::Texture2D);
	textureStream->setFile(options.resourceLocation + "test.png", loadImage);
	textureStream->setFiltering(mpp::TextureParams::MinFilter::Linear, mpp::TextureParams::MagFilter::Linear);
	addResource(resourceMgr->declareResource("Test.Texture", ResourceStreamPtr(textureStream)).first, false);

	// Milestone 2 texture-system smoke resource. A neutral colour is repeated
	// across cube faces until environment preprocessing is added in Milestone 4.
	textureStream = new ProgrammaticTextureStream(resourceMgr);
	textureStream->setTarget(TextureTarget::CubeMap);
	textureStream->setData([](string const&)
	{
		TextureData data;
		data.width = 1;
		data.height = 1;
		data.bitsPerPixel = 24;
		data.dataType = GL_UNSIGNED_BYTE;
		data.pixelFormat = GL_RGB;
		data.data = new uint8_t[3]{ 128, 160, 255 };
		return data;
	});
	textureStream->setFiltering(mpp::TextureParams::MinFilter::LinearMipmapLinear, mpp::TextureParams::MagFilter::Linear);
	textureStream->setWrapping(mpp::TextureParams::Wrapping::ClampToEdge);
	textureStream->setColourSpace(mpp::TextureColourSpace::Srgb);
	textureStream->enableMipMaps(true);
	addResource(resourceMgr->declareResource("PBR.Preview.Environment", ResourceStreamPtr(textureStream)).first, true);

	// Alternate warm environment for the PBR validation controls. Both are
	// precomputed cube-map placeholders until HDR panorama preprocessing exists.
	textureStream = new ProgrammaticTextureStream(resourceMgr);
	textureStream->setTarget(TextureTarget::CubeMap);
	textureStream->setData([](string const&)
	{
		TextureData data;
		data.width = 1;
		data.height = 1;
		data.bitsPerPixel = 24;
		data.dataType = GL_UNSIGNED_BYTE;
		data.pixelFormat = GL_RGB;
		data.data = new uint8_t[3]{ 255, 176, 104 };
		return data;
	});
	textureStream->setFiltering(mpp::TextureParams::MinFilter::LinearMipmapLinear, mpp::TextureParams::MagFilter::Linear);
	textureStream->setWrapping(mpp::TextureParams::Wrapping::ClampToEdge);
	textureStream->setColourSpace(mpp::TextureColourSpace::Srgb);
	textureStream->enableMipMaps(true);
	addResource(resourceMgr->declareResource("PBR.Preview.EnvironmentWarm", ResourceStreamPtr(textureStream)).first, true);

	textureStream = new ProgrammaticTextureStream(resourceMgr);
	textureStream->setTarget(TextureTarget::Texture2D);
	textureStream->setData([](string const&)
	{
		TextureData data;
		data.width = 1;
		data.height = 1;
		data.bitsPerPixel = 16;
		data.dataType = GL_UNSIGNED_BYTE;
		data.pixelFormat = GL_RG;
		data.data = new uint8_t[2]{ 255, 255 };
		return data;
	});
	textureStream->setFiltering(mpp::TextureParams::MinFilter::Linear, mpp::TextureParams::MagFilter::Linear);
	addResource(resourceMgr->declareResource("PBR.Preview.BrdfLut", ResourceStreamPtr(textureStream)).first, true);

	textureStream = new ProgrammaticTextureStream(resourceMgr);
	textureStream->setTarget(TextureTarget::Texture2D);
	textureStream->setFile(options.resourceLocation + "dragon.png", loadImage);
	textureStream->setFiltering(mpp::TextureParams::MinFilter::Nearest, mpp::TextureParams::MagFilter::Nearest);
	addResource(resourceMgr->declareResource("Dragon.Texture", ResourceStreamPtr(textureStream)).first, false);

	textureStream = new ProgrammaticTextureStream(resourceMgr);
	textureStream->setAtlas(true);
	textureStream->setTarget(TextureTarget::Texture2D);
	textureStream->setFile(options.resourceLocation + "bullets.png", loadImage);
	textureStream->setFiltering(mpp::TextureParams::MinFilter::Nearest, mpp::TextureParams::MagFilter::Nearest);
	addResource(resourceMgr->declareResource("Bullets.Texture", ResourceStreamPtr(textureStream)).first, false);

	textureStream = new ProgrammaticTextureStream(resourceMgr);
	textureStream->setTarget(TextureTarget::Texture2D);
	textureStream->setFile(options.resourceLocation + "atlas.png", loadImage);
	textureStream->setFiltering(mpp::TextureParams::MinFilter::Nearest, mpp::TextureParams::MagFilter::Nearest);
	addResource(resourceMgr->declareResource("Atlas.Texture", ResourceStreamPtr(textureStream)).first, false);

	// Create texture from file definition.
	auto fileStream = new resource_parsers::FileTextureStream(resourceMgr, options.resourceLocation + "Doughnut.xml");
	addResource(resourceMgr->declareResource("Doughnut.Texture", ResourceStreamPtr(fileStream)).first, false);

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
	addResource(resourceMgr->declareResource("Strip.Texture", ResourceStreamPtr(textureStream)).first, false);
}

//
// Mesh specifications and materials
//
mesh::MeshSpecification ModelScene::createBatch2dMeshSpecification()
{
	mesh::MeshSpecification meshSpec(mesh::Primitive::Type::Triangles);

	mesh::VertexBufferAttributeLayout* attribLayout = meshSpec.createVertexBufferAttributeLayout(false);
	attribLayout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);

	attribLayout = meshSpec.createVertexBufferAttributeLayout(true);
	attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::UnsignedByte, true);

	meshSpec.setStorageType(mesh::VertexBufferStorageType::Static);
	meshSpec.setIndexedVertices(false);

	return meshSpec;
}

mesh::MeshSpecification ModelScene::createBatch3dMeshSpecification()
{
	mesh::MeshSpecification meshSpec(mesh::Primitive::Type::Triangles);

	mesh::VertexBufferAttributeLayout* attribLayout = meshSpec.createVertexBufferAttributeLayout(false);
	attribLayout->createAttribute(mesh::Vertex::Component::Position3, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::Normal3, mesh::Vertex::DataType::Float, false);

	attribLayout = meshSpec.createVertexBufferAttributeLayout(true);
	attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
	attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::UnsignedByte, true);

	meshSpec.setStorageType(mesh::VertexBufferStorageType::Static);
	meshSpec.setIndexedVertices(false);

	return meshSpec;
}

void ModelScene::createBatchMaterials(mpp::mesh::MeshSpecification const& spec2d, mpp::mesh::MeshSpecification const& spec3d, ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	auto materialStream = new ProgrammaticBasicMaterialStream(resourceMgr);
	materialStream->setProgram2d(true);
	materialStream->setMeshSpecification(spec2d);
	materialStream->setTexture("TEX1", "__mpp_tex_none__");

	addResource(resourceMgr->declareResource("Default.Material", ResourceStreamPtr(materialStream)).first, true);

	materialStream = new ProgrammaticBasicMaterialStream(resourceMgr);
	materialStream->setProgram2d(true);
	materialStream->setMeshSpecification(spec2d);
	materialStream->setTexture("TEX1", "Test.Texture");

	addResource(resourceMgr->declareResource("Batch.2D.Material", ResourceStreamPtr(materialStream)).first, true);

	materialStream = new ProgrammaticBasicMaterialStream(resourceMgr);
	materialStream->setProgram2d(true);
	materialStream->setMeshSpecification(spec2d);
	materialStream->setTexture("TEX1", "Bullets.Texture");

	addResource(resourceMgr->declareResource("Bullets.Material", ResourceStreamPtr(materialStream)).first, true);

	materialStream = new ProgrammaticBasicMaterialStream(resourceMgr);
	materialStream->setProgram2d(false);
	materialStream->setMeshSpecification(spec3d);
	materialStream->setTexture("TEX1", "Test.Texture");

	addResource(resourceMgr->declareResource("Batch.3D.Material", ResourceStreamPtr(materialStream)).first, true);
}

//
// 3d Heightmap with height generated by vertex shader
//
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

void ModelScene::createGridMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	auto materialStream = new ProgrammaticBasicMaterialStream(resourceMgr);
	materialStream->setProgram2d(false);
	materialStream->setMeshSpecification(meshSpec);
	// A flat legacy-lit receiver for the generic shadow demonstration.
	materialStream->setTexture("TEX1", "Marble.Texture");

	ResourceStreamPtr matStreamPtr(materialStream);
	addResource(resourceMgr->declareResource("Grid.Material", matStreamPtr).first, true);
}

//
// Textured sphere primitive
//
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

void ModelScene::createSphereMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	auto materialStream = resource_parsers::FileMaterialStream::fromFile(resourceMgr, options.resourceLocation + "ElectricMaterial.xml");
	addResource(resourceMgr->declareResource("Sphere.Material", materialStream).first, true);
}

//
// Textured cylinder primitive
//
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

void ModelScene::createCylinderMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	auto materialStream = new ProgrammaticBasicMaterialStream(resourceMgr);
	materialStream->setProgram2d(false);
	materialStream->setMeshSpecification(meshSpec);
	materialStream->setTexture("TEX1", "Marble.Texture");

	addResource(resourceMgr->declareResource("Cylinder.Material", ResourceStreamPtr(materialStream)).first, true);
}

//
// Textured cube primitive
//
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

void ModelScene::createBoxMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	auto materialStream = new ProgrammaticBasicMaterialStream(resourceMgr);
	materialStream->setProgram2d(false);
	materialStream->setMeshSpecification(meshSpec);
	materialStream->setTexture("TEX1", "Test.Texture");

	addResource(resourceMgr->declareResource("Box.Material", ResourceStreamPtr(materialStream)).first, true);
}

//
// Textured torus primitive
//
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

void ModelScene::createTorusMaterial(mpp::mesh::MeshSpecification const& meshSpec, ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	auto materialStream = new ProgrammaticBasicMaterialStream(resourceMgr);
	materialStream->setProgram2d(false);
	materialStream->setMeshSpecification(meshSpec);
	materialStream->setTexture("TEX1", "Doughnut.Texture");

	addResource(resourceMgr->declareResource("Torus.Material", ResourceStreamPtr(materialStream)).first, true);
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

			torusStream->addTriangle(torusMeshId, (uint32_t)i0, (uint32_t)i1, (uint32_t)i2);
			torusStream->addTriangle(torusMeshId, (uint32_t)i2, (uint32_t)i3, (uint32_t)i0);
		}
	}

	mTorus = resourceMgr->declareResource("Model.Torus", ResourceStreamPtr(torusStream)).first;
	mTorus->acquire(this);
	mTorus->load();

	return mTorus;
}

//
// 2d Batches
//
void ModelScene::createBatches(mpp::RenderSystem* renderSystem)
{
	const int tilesX{ 4 };
	const int tilesY{ 2 };

	auto tileWidth = renderSystem->getWindowWidth() / tilesX;
	auto tileHeight = renderSystem->getWindowHeight() / tilesY;

	struct Tile
	{
		int x0, y0, x1, y1, cx, cy;
		int tx, ty;
	};

	vector<Tile> tiles;
	for (int y = 0; y < tilesY; ++y)
	{
		for (int x = 0; x < tilesX; ++x)
		{
			Tile t{
				(int)(x * tileWidth), (int)(y * tileHeight),
				(int)((x + 1) * tileWidth), (int)((y + 1) * tileHeight),
				(int)((x + 0.5f) * tileWidth), (int)((y + 0.5f) * tileHeight),
				(int)(x * tileWidth + 32), (int)((y + 1) * tileHeight)
			};

			tiles.push_back(t);
		}
	}

	int batchRenderOrder = 0;
	auto resourceMgr = getResourceManager();

	//
	// Animated lines
	//
	auto const* tile = &tiles[0];

	mpp::helper::LineBatchRendererParams lineParams
	{
		true,
		true,
		false
	};

	auto lineBatchDataProvider = make_shared<TestLineBatchDataProvider>(
		tile->x0,
		tile->y0,
		tile->x1 - tile->x0,
		tile->ty - tile->y0);

	auto lineBatchRenderer = make_shared<mpp::helper::LineBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>>(
		"TestLines",
		lineParams,
		lineBatchDataProvider,
		renderSystem,
		resourceMgr);

	lineBatchRenderer->create();

	m2dBatches[0] = {
		getScene()->add2dBatch(lineBatchDataProvider, lineBatchRenderer, batchRenderOrder++),
		tile->tx, tile->ty,
		"Lines (per-line colour)"
	};

	//
	// Animated triangles
	//
	tile = &tiles[1];

	mpp::helper::TriangleBatchRendererParams triParams
	{
		true,
		true,
		true,
		false,
		false
	};

	auto tri2dBatchDataProvider = make_shared<Test2dTriangleBatchDataProvider>(
		tile->x0,
		tile->y0,
		tile->x1 - tile->x0,
		tile->ty - tile->y0);

	auto tri2dBatchRenderer = make_shared<mpp::helper::TriangleBatch2DRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>>(
		"TestTrisBatch",
		triParams,
		tri2dBatchDataProvider,
		resourceMgr->getResource("Default.Material"),
		renderSystem,
		resourceMgr);

	tri2dBatchRenderer->create();

	m2dBatches[1] = {
		getScene()->add2dBatch(tri2dBatchDataProvider, tri2dBatchRenderer, batchRenderOrder++),
		tile->tx, tile->ty,
		"Triangles (unindexed)"
	};

	//
	// Bullets, rotating by angle
	//
	tile = &tiles[2];

	mpp::helper::QuadBatchRendererParams bullet1Params(
		mpp::QuadBatchOptions::PrimitiveOptions::Auto,
		mpp::QuadBatchOptions::RotationOptions::Angle,
		true,  // fixed texcoords
		true,  // fixed colour (no colour, in fact)
		false, // don't use vertex colours
		true,  // use diffuse colour
		24,    // width
		24,    // height
		true,  // square
		16,    // 16-bit indices
		resourceMgr->getResource("Bullets.Texture"));

	auto bullets1BatchDataProvider = make_shared<BulletsByAngleQuadBatchDataProvider>(
		tile->x0,
		tile->y0,
		tile->x1 - tile->x0,
		tile->ty - tile->y0,
		16);

	auto bullets1BatchRenderer = make_shared<mpp::helper::QuadBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>>(
		"TestQuads3",
		bullet1Params,
		bullets1BatchDataProvider,
		renderSystem,
		resourceMgr);

	bullets1BatchRenderer->create();

	m2dBatches[2] = {
		getScene()->add2dBatch(bullets1BatchDataProvider, bullets1BatchRenderer, batchRenderOrder++),
		tile->tx, tile->ty,
		"Quads (uv-rotate by angle)"
	};

	//
	// Bullets, rotating by direction
	//
	tile = &tiles[3];

	mpp::helper::QuadBatchRendererParams bullet2Params(
		mpp::QuadBatchOptions::PrimitiveOptions::Auto,
		mpp::QuadBatchOptions::RotationOptions::Direction,
		true,  // fixed texcoords
		true,  // fixed colour (no colour, in fact)
		false, // don't use vertex colours
		true,  // use diffuse colour
		24,    // width
		24,    // height
		true,  // square
		16,    // 16-bit indices
		resourceMgr->getResource("Bullets.Texture"));

	auto bullets2BatchDataProvider = make_shared<BulletsByDirQuadBatchDataProvider>(
		tile->x0,
		tile->y0,
		tile->x1 - tile->x0,
		tile->ty - tile->y0,
		16);

	auto bullets2BatchRenderer = make_shared<mpp::helper::QuadBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>>(
		"TestQuads4",
		bullet2Params,
		bullets2BatchDataProvider,
		renderSystem,
		resourceMgr);

	bullets2BatchRenderer->create();

	m2dBatches[3] = {
		getScene()->add2dBatch(bullets2BatchDataProvider, bullets2BatchRenderer, batchRenderOrder++),
		tile->tx, tile->ty,
		"Quads (uv-rotate by dir)"
	};

	//
	// Large rectangles, rotating by angle
	//
	tile = &tiles[4];

	auto dragonTexture = resourceMgr->getResource("Dragon.Texture");
	dragonTexture->load();
	mpp::helper::QuadBatchRendererParams dragonParams(
		mpp::QuadBatchOptions::PrimitiveOptions::Triangles,
		mpp::QuadBatchOptions::RotationOptions::Angle,
		true,   // fixed texcoords
		true,   // fixed colour (no colour, in fact)
		false,  // don't use vertex colours
		false,  // use diffuse colour
		static_cast<Texture const*>(dragonTexture.get())->getWidth(),
		static_cast<Texture const*>(dragonTexture.get())->getHeight(),
		false,  // not square
		16,     // 16-bit indices
		dragonTexture);

	auto dragonDataProvider = make_shared<TestDragonDataProvider>(
		tile->x0,
		tile->y0,
		tile->x1 - tile->x0,
		tile->ty - tile->y0);

	auto dragonRenderer = make_shared<mpp::helper::QuadBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>>(
		"TestQuads5",
		dragonParams,
		dragonDataProvider,
		renderSystem,
		resourceMgr);

	dragonRenderer->create();

	m2dBatches[4] = {
		getScene()->add2dBatch(dragonDataProvider, dragonRenderer, batchRenderOrder++),
		tile->tx, tile->ty,
		"Rects (vert-rotate by angle)"
	};

	//
	// Large rectangles, rotating by direction
	//
	tile = &tiles[5];

	mpp::helper::QuadBatchRendererParams dragon2Params(
		mpp::QuadBatchOptions::PrimitiveOptions::Triangles,
		mpp::QuadBatchOptions::RotationOptions::Direction,
		true,   // fixed texcoords
		true,   // fixed colour (no colour, in fact)
		false,  // don't use vertex colours
		false,  // use diffuse colour
		static_cast<Texture const*>(dragonTexture.get())->getWidth(),
		static_cast<Texture const*>(dragonTexture.get())->getHeight(),
		false,  // not square
		16,     // 16-bit indices
		dragonTexture);

	auto dragon2DataProvider = make_shared<TestDragonDataProvider>(
		tile->x0,
		tile->y0,
		tile->x1 - tile->x0,
		tile->ty - tile->y0);

	auto dragon2Renderer = make_shared<mpp::helper::QuadBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat>>(
		"TestQuads6",
		dragon2Params,
		dragon2DataProvider,
		renderSystem,
		resourceMgr);

	dragon2Renderer->create();

	m2dBatches[5] = {
		getScene()->add2dBatch(dragon2DataProvider, dragon2Renderer, batchRenderOrder++),
		tile->tx, tile->ty,
		"Rects(vert - rotate by dir)"
	};
}

void ModelScene::setupImpl(mpp::RenderSystem* renderSystem, ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();
	auto mppScene = getScene();

	createSharedTextures(options);
	createBatchMaterials(createBatch2dMeshSpecification(), createBatch3dMeshSpecification(), options);

	//
	// 3d renderers
	//

	m3dBatchDataProvider = make_shared<Test3dTriangleBatchDataProvider>();
	m3dBatchBufferDataProvider = make_shared<Test3dTriangleBatchBufferDataProvider>();

	mpp::helper::TriangleBatchRendererParams triParams
	{
		true,
		true,
		true,
		false
	};

	m3dTestRenderer = make_shared<mpp::helper::TriangleBatch3DRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>>(
		"Test3dBatch",
		triParams,
		m3dBatchDataProvider,
		resourceMgr->getResource("Batch.3D.Material"),
		renderSystem,
		resourceMgr);

	m3dTestRenderer->create();

	mpp::helper::TriangleBatchRendererParams bufferTriParams
	{
		true,
		false,
		false,
		false,
		true
	};

	m3dTestBufferRenderer = make_shared<mpp::helper::TriangleBatch3DBufferRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>>(
		"Test3dBufferBatch",
		bufferTriParams,
		m3dBatchBufferDataProvider,
		16,
		resourceMgr->getResource("Batch.3D.Material"),
		renderSystem,
		resourceMgr);

	m3dTestBufferRenderer->create();

	mModels.push_back(getScene()->add3dModel(m3dTestBufferRenderer->getModel()));

	mModels.push_back(getScene()->add3dModel(m3dTestRenderer->getModel()));
	mModels.back()->getParams()->setModelFlags(mModels.back()->getParams()->getModelFlags() & ~mpp::ModelRenderParams::Flag_Visible);

	// Load Grid
	auto gridMeshSpec = createGridMeshSpecification();
	createGridMaterial(gridMeshSpec, options);

	auto gridStream = new GridModelStream(resourceMgr, gridMeshSpec, "Grid.Material", 1600, 1600, 32, 32);
	mGrid = resourceMgr->declareResource("Model.Grid", ResourceStreamPtr(gridStream)).first;
	mGrid->acquire(this);
	mGrid->load();

	mModels.push_back(mppScene->add3dModel(mGrid));
	// The floor and walls receive shadows but do not contribute depth as casters.
	mModels.back()->getParams()->setModelFlags(mpp::ModelRenderParams::Flag_Visible);

	// Four inward-facing single-plane walls surround the statue. They reuse the
	// legacy-lit grid material so PBR casters can visibly shadow non-PBR receivers.
	auto addShadowWall = [&](glm::vec3 const& position, float angle, glm::vec3 const& axis)
	{
		auto wall = mppScene->add3dModel(mGrid);
		wall->getParams()->setModelFlags(mpp::ModelRenderParams::Flag_Visible | mpp::ModelRenderParams::Flag_CullBackFaces);
		wall->translate(position);
		wall->rotateSelf(angle, axis);
		mShadowWalls.push_back(wall);
	};
	constexpr float halfPi = 1.57079632679f;
	addShadowWall(glm::vec3(0.0f, 800.0f, -800.0f), halfPi, glm::vec3(1.0f, 0.0f, 0.0f));
	addShadowWall(glm::vec3(0.0f, 800.0f, 800.0f), -halfPi, glm::vec3(1.0f, 0.0f, 0.0f));
	addShadowWall(glm::vec3(-800.0f, 800.0f, 0.0f), -halfPi, glm::vec3(0.0f, 0.0f, 1.0f));
	addShadowWall(glm::vec3(800.0f, 800.0f, 0.0f), halfPi, glm::vec3(0.0f, 0.0f, 1.0f));

	// Load Sphere
	auto sphereMeshSpec = createSphereMeshSpecification();
	createSphereMaterial(sphereMeshSpec, options);

	auto sphereStream = new SphereModelStream(resourceMgr, sphereMeshSpec, "Sphere.Material", 40, 4);
	mSphere = resourceMgr->declareResource("Model.Sphere", ResourceStreamPtr(sphereStream)).first;
	mSphere->acquire(this);
	mSphere->load();

	auto sphereModel = mppScene->add3dModel(mSphere);
	mModels.push_back(sphereModel);

	auto sphereParams = sphereModel->getParams();
	sphereParams->setModelFlags(sphereParams->getModelFlags() & ~mpp::ModelRenderParams::Flag_Visible);
	sphereParams->addModelRenderCommand({ 0, 400, nullptr, { nullptr, nullptr } });
	sphereParams->addModelRenderCommand({ 400, 400, nullptr, { resourceMgr->getResource("Test.Texture"), nullptr } });

	// Load Cylinder
	auto cylinderMeshSpec = createCylinderMeshSpecification();
	createCylinderMaterial(cylinderMeshSpec, options);

	auto cylinderStream = new CylinderModelStream(resourceMgr, cylinderMeshSpec, "Cylinder.Material", 80, 24, 24, 16);
	mCylinder = resourceMgr->declareResource("Model.Cylinder", ResourceStreamPtr(cylinderStream)).first;
	mCylinder->acquire(this);
	mCylinder->load();

	mModels.push_back(mppScene->add3dModel(mCylinder));
	mModels.back()->getParams()->setModelFlags(mModels.back()->getParams()->getModelFlags() & ~mpp::ModelRenderParams::Flag_Visible);

	// Load Box
	auto boxMeshSpec = createBoxMeshSpecification();
	createBoxMaterial(boxMeshSpec, options);

	auto boxStream = new BoxModelStream(resourceMgr, boxMeshSpec, "Box.Material", 32, 32, 32);
	mBox = resourceMgr->declareResource("Model.Box", ResourceStreamPtr(boxStream)).first;
	mBox->acquire(this);
	mBox->load();

	mModels.push_back(mppScene->add3dModel(mBox));
	mModels.back()->getParams()->setModelFlags(mModels.back()->getParams()->getModelFlags() & ~mpp::ModelRenderParams::Flag_Visible);

	// A separate, unlit material makes the light marker visible without
	// receiving the generic shadow sampler.
	auto lightMarkerMaterialStream = new ProgrammaticBasicMaterialStream(resourceMgr);
	lightMarkerMaterialStream->setProgram2d(false);
	lightMarkerMaterialStream->setMeshSpecification(boxMeshSpec);
	lightMarkerMaterialStream->setProgramFragmentShaderFile(options.resourceLocation + "LightMarker.frag");
	auto lightMarkerMaterial = resourceMgr->declareResource("Light.Marker.Material", ResourceStreamPtr(lightMarkerMaterialStream)).first;
	addResource(lightMarkerMaterial, true);

	mLightMarker = mppScene->add3dModel(mBox);
	mLightMarker->getParams()->setModelMaterial(lightMarkerMaterial);
	mLightMarker->getParams()->setModelFlags(mpp::ModelRenderParams::Flag_Visible);
	mLightMarker->translate(mLightPosition);
	mLightMarker->scale(glm::vec3(0.5f));

	// A legacy-lit cube behind the PBR statue exercises bidirectional
	// mixed-material casting/receiving through the shared shadow domain.
	mShadowCube = mppScene->add3dModel(mBox);
	mShadowCube->translate(glm::vec3(0.0f, 32.0f, -160.0f));
	mShadowCube->scale(glm::vec3(2.0f));

	// Load torus
	createTorusModel(options);

	mModels.push_back(mppScene->add3dModel(mTorus));
	mModels.back()->getParams()->setModelFlags(mModels.back()->getParams()->getModelFlags() & ~mpp::ModelRenderParams::Flag_Visible);

	// Load the PBR preview model. It remains visible by default and is rendered
	// through the opt-in PBR pipeline below.
	auto statueStream = new MppModelStream(resourceMgr, options.resourceLocation + "statue/statue.mppmodel");
	mStatue = resourceMgr->declareResource("Model.Statue", ResourceStreamPtr(statueStream)).first;
	mStatue->acquire(this);
	mStatue->load();

	// Real-context built-in specialization/reflection/cache contract smoke test.
	// Reuse the statue's canonical PBR mesh layout while compiling engine-owned
	// minimal and fully featured fragment variants.
	auto statueModel = static_cast<mpp::Model*>(mStatue.get());
	auto statueMaterial = static_cast<mpp::PbrMaterial*>(statueModel->getMesh(0)->getMaterial().get());
	auto statueProgram = static_cast<mpp::Program*>(statueMaterial->getProgram().get());
	auto createBuiltInVariant = [&](std::string const& name, mpp::PbrMaterialSpecification::PbrSurface const& surface, bool maps)
	{
		auto stream = new mpp::ProgrammaticPbrMaterialStream(resourceMgr);
		stream->setMeshSpecification(statueProgram->getMeshSpecification());
		stream->setSurface(surface);
		if (maps)
		{
			stream->setBaseColourMap("__mpp_tex_pbr_white__");
			stream->setMetallicRoughnessMap("__mpp_tex_pbr_metallic_roughness__");
			stream->setNormalMap("__mpp_tex_pbr_normal__");
			stream->setOcclusionMap("__mpp_tex_pbr_white__");
			stream->setEmissiveMap("__mpp_tex_pbr_white__");
		}
		auto resource = resourceMgr->declareResource(name, mpp::ResourceStreamPtr(stream)).first;
		addResource(resource, true);
		return static_cast<mpp::PbrMaterial*>(resource.get());
	};
	mpp::PbrMaterialSpecification::PbrSurface minimalSurface;
	minimalSurface.metallicFactor = 0.0f;
	minimalSurface.roughnessFactor = 0.0f;
	minimalSurface.normalScale = 0.0f;
	minimalSurface.occlusionStrength = 0.0f;
	auto minimalPbr = createBuiltInVariant("PBR.Specialization.Minimal", minimalSurface, false);
	if (minimalPbr->getFeatures() != 0) throw std::runtime_error("Minimal built-in PBR specialization selected unexpected features.");
	auto minimalProgram = static_cast<mpp::Program*>(minimalPbr->getProgram().get());
	if (minimalProgram->getUniformId("PBR_METALLIC_FACTOR") >= 0 || minimalProgram->getSamplerGlType("PBR_NORMAL_MAP") != 0)
		throw std::runtime_error("Minimal built-in PBR specialization retained disabled inputs.");
	mpp::PbrMaterialSpecification::PbrSurface fullSurface;
	fullSurface.metallicFactor = 1.0f;
	fullSurface.roughnessFactor = 1.0f;
	fullSurface.normalScale = 1.0f;
	fullSurface.occlusionStrength = 1.0f;
	fullSurface.emissiveFactor = glm::vec3(1.0f);
	fullSurface.alphaMode = mpp::PbrMaterialSpecification::PbrAlphaMode::Mask;
	fullSurface.doubleSided = true;
	auto fullPbr = createBuiltInVariant("PBR.Specialization.Full", fullSurface, true);
	auto fullSurfaceDifferentValues = fullSurface;
	fullSurfaceDifferentValues.roughnessFactor = 0.25f;
	fullSurfaceDifferentValues.emissiveFactor = glm::vec3(0.5f);
	auto fullPbrSecond = createBuiltInVariant("PBR.Specialization.FullCache", fullSurfaceDifferentValues, true);
	if (fullPbr->getProgram() != fullPbrSecond->getProgram()) throw std::runtime_error("Equivalent PBR specialization variants did not share a cached program.");
	if (fullPbr->getProgram() == minimalPbr->getProgram()) throw std::runtime_error("Different PBR specialization masks shared one program.");
	auto blendSurface = minimalSurface;
	blendSurface.alphaMode = mpp::PbrMaterialSpecification::PbrAlphaMode::Blend;
	auto blendPbr = createBuiltInVariant("PBR.Specialization.Blend", blendSurface, false);
	if (!mpp::hasPbrFeature(blendPbr->getFeatures(), mpp::PbrMaterialFeature::AlphaBlend) || blendPbr->getProgram() == minimalPbr->getProgram())
		throw std::runtime_error("Blend PBR specialization did not select a distinct static-alpha variant.");
	mpp::UniformCollection zeroMetallic;
	zeroMetallic.setUniform("PBR_METALLIC_FACTOR", 0.0f);
	fullPbr->validateInstanceUniforms(zeroMetallic);
	bool disabledOverrideRejected = false;
	try { minimalPbr->validateInstanceUniforms(zeroMetallic); } catch (...) { disabledOverrideRejected = true; }
	if (!disabledOverrideRejected) throw std::runtime_error("Instance override enabled a specialized-out PBR feature.");
	auto expectContractFailure = [&](std::string const& name, mpp::PbrMaterialSpecification::PbrSurface const& surface, bool maps, std::string const& programName, std::string const& expected)
	{
		auto stream = new mpp::ProgrammaticPbrMaterialStream(resourceMgr);
		stream->setProgram(programName);
		stream->setSurface(surface);
		if (maps)
		{
			stream->setBaseColourMap("__mpp_tex_pbr_white__");
			stream->setMetallicRoughnessMap("__mpp_tex_pbr_metallic_roughness__");
			stream->setNormalMap("__mpp_tex_pbr_normal__");
			stream->setOcclusionMap("__mpp_tex_pbr_white__");
			stream->setEmissiveMap("__mpp_tex_pbr_white__");
		}
		auto resource = resourceMgr->declareResource(name, mpp::ResourceStreamPtr(stream)).first;
		try { resource->load(); }
		catch (std::exception const& exception) { if (std::string(exception.what()).find(expected) != std::string::npos) return; throw; }
		throw std::runtime_error("Expected PBR custom contract failure was accepted: " + name);
	};
	expectContractFailure("PBR.Specialization.MissingContract", fullSurface, true, minimalPbr->getProgram()->getName(), "missing required uniform");
	expectContractFailure("PBR.Specialization.UnexpectedContract", minimalSurface, false, fullPbr->getProgram()->getName(), "specialized-out uniform");
	auto replaceAll = [](std::string source, std::string const& from, std::string const& to)
	{
		for (size_t offset = 0; (offset = source.find(from, offset)) != std::string::npos; offset += to.size()) source.replace(offset, from.size(), to);
		return source;
	};
	auto expectSourceContractFailure = [&](std::string const& name, std::string const& fragmentSource, std::string const& expected)
	{
		auto sourceStream = new mpp::ProgrammaticStringStream(resourceMgr);
		sourceStream->setString(fragmentSource);
		auto sourceResource = resourceMgr->declareResource(name + ".Source", mpp::ResourceStreamPtr(sourceStream)).first;
		addResource(sourceResource, true);
		auto stream = new mpp::ProgrammaticPbrMaterialStream(resourceMgr);
		stream->setMeshSpecification(statueProgram->getMeshSpecification());
		stream->setProgramFragmentShaderResource(name + ".Source");
		stream->setSurface(fullSurface);
		stream->setBaseColourMap("__mpp_tex_pbr_white__");
		stream->setMetallicRoughnessMap("__mpp_tex_pbr_metallic_roughness__");
		stream->setNormalMap("__mpp_tex_pbr_normal__");
		stream->setOcclusionMap("__mpp_tex_pbr_white__");
		stream->setEmissiveMap("__mpp_tex_pbr_white__");
		auto resource = resourceMgr->declareResource(name, mpp::ResourceStreamPtr(stream)).first;
		try { resource->load(); }
		catch (std::exception const& exception) { if (std::string(exception.what()).find(expected) != std::string::npos) return; throw; }
		throw std::runtime_error("Expected PBR source contract failure was accepted: " + name);
	};
	std::string wrongUniformType = mpp::BuiltInPbrFragmentShader;
	wrongUniformType = replaceAll(wrongUniformType, "@@Uniform(float PBR_METALLIC_FACTOR);", "@@Uniform(int PBR_METALLIC_FACTOR);");
	wrongUniformType = replaceAll(wrongUniformType, "@Uniform(PBR_METALLIC_FACTOR)", "float(@Uniform(PBR_METALLIC_FACTOR))");
	expectSourceContractFailure("PBR.Specialization.WrongUniformType", wrongUniformType, "wrong GLSL type");
	std::string wrongSamplerTarget = mpp::BuiltInPbrFragmentShader;
	wrongSamplerTarget = replaceAll(wrongSamplerTarget, "@@Texture(sampler2D PBR_NORMAL_MAP);", "@@Texture(samplerCube PBR_NORMAL_MAP);");
	wrongSamplerTarget = replaceAll(wrongSamplerTarget, "texture(@Texture(PBR_NORMAL_MAP), @In(TEXCOORDS))", "texture(@Texture(PBR_NORMAL_MAP), vec3(@In(TEXCOORDS), 1.0))");
	expectSourceContractFailure("PBR.Specialization.WrongSamplerTarget", wrongSamplerTarget, "wrong sampler type");
	renderSystem->infoMessage("Built-in PBR reflection/cache, exact custom contracts, and instance-boundary tests passed.");

	// The old preview material mixed PBR data into the generic material path.
	// Phase 2 replaces it with a dedicated PbrMaterial resource.

	mModels.push_back(mppScene->add3dModel(mStatue));
	mPbrStatueUniforms = make_shared<UniformCollection>();
	mPbrStatueUniforms->setUniform("PBR_BASE_COLOUR_FACTOR", mPbrBaseColour);
	mPbrStatueUniforms->setUniform("PBR_METALLIC_FACTOR", mPbrMetallic);
	mPbrStatueUniforms->setUniform("PBR_ROUGHNESS_FACTOR", mPbrRoughness);
	mModels.back()->getParams()->setModelUniforms(mPbrStatueUniforms);
	//mModels.back()->getParams()->setModelMaterial(mPbrPreviewMaterial);
	//mModels.back()->getParams()->setModelFlags(mModels.back()->getParams()->getModelFlags() & ~mpp::ModelRenderParams::Flag_Visible);

	// Batches
	createBatches(renderSystem);

	// ImGui rendering
	vector<mpp::ResourcePtr> imGuiTextures;
	imGuiTextures.push_back(resourceMgr->getResource("__ImGui_Font__"));

	mImGuiDataProvider = make_shared<ImGuiDataProvider>(imGuiTextures);
	mImGuiRenderer = new mpp::BufferRenderer(mImGuiDataProvider);

	// Lighting
	renderSystem->setAmbientColour(Colour::Grey25);
	renderSystem->setLightCount(1);
	renderSystem->setLight1Colour(Colour::White);

	mpp::PbrLight pbrLight;
	pbrLight.type = mpp::PbrLightType::Directional;
	pbrLight.direction = mShadowOptions.light.direction;
	pbrLight.colour = glm::vec3(1.0f);
	pbrLight.intensity = mPbrLightIntensity;
	pbrLight.range = 0.0f;
	renderSystem->setPbrAmbientColour(Colour(0.03f, 0.03f, 0.03f));
	renderSystem->setPbrLights({ pbrLight });

	// PBR is an opt-in pipeline. Milestone 1 uses the statue as the visible
	// HDR preview while later milestones replace its temporary shading path.
	mShadowOptions.enabled = true;
	mShadowOptions.resolution = 1024;
	mShadowOptions.orthoHalfWidth = 1000.0f;
	mShadowOptions.light.direction = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
	renderSystem->configureShadowDomain("DemoSuite.MainDirectionalShadow", mShadowOptions);

	mBloomOptions.enabled = true;
	mBloomOptions.threshold = 0.7f;
	mBloomOptions.intensity = 0.2f;
	mBloomOptions.blurPasses = 2;

	mpp::RenderPipelineOptions pbrOptions;
	pbrOptions.mode = mpp::RenderPipelineMode::PbrForward;
	pbrOptions.bloom = mBloomOptions;
	pbrOptions.shadowDomain = "DemoSuite.MainDirectionalShadow";
	mPbrEnvironment = make_shared<mpp::PbrEnvironment>();
	mPbrEnvironment->irradianceMap = resourceMgr->getResource("PBR.Preview.Environment");
	mPbrEnvironment->prefilteredSpecularMap = resourceMgr->getResource("PBR.Preview.Environment");
	mPbrEnvironment->brdfIntegrationLut = resourceMgr->getResource("PBR.Preview.BrdfLut");
	mPbrEnvironment->backgroundMap = resourceMgr->getResource("PBR.Preview.Environment");
	pbrOptions.environment = mPbrEnvironment;
	renderSystem->getOrCreateRenderPipeline("PBR", pbrOptions);
	auto graphPbrOptions = pbrOptions;
	graphPbrOptions.mode = mpp::RenderPipelineMode::GraphPbrForward;
	renderSystem->getOrCreateRenderPipeline("GraphPBR", graphPbrOptions);
	auto xmlGraphStream = new mpp::resource_parsers::FileRenderGraphStream(resourceMgr, options.resourceLocation + "PbrPipeline.rendergraph.xml");
	auto xmlGraph = resourceMgr->declareResource("PBR.XmlGraph", mpp::ResourceStreamPtr(xmlGraphStream)).first;
	auto xmlMrtGraphStream = new mpp::resource_parsers::FileRenderGraphStream(resourceMgr, options.resourceLocation + "PbrPipelineMrt.rendergraph.xml");
	auto xmlMrtGraph = resourceMgr->declareResource("PBR.XmlGraphMrt", mpp::ResourceStreamPtr(xmlMrtGraphStream)).first;
	auto xmlPbrOptions = pbrOptions;
	xmlPbrOptions.mode = mpp::RenderPipelineMode::XmlGraphPbrForward;
	xmlPbrOptions.graphTemplate = xmlGraph;
	xmlPbrOptions.graphTemplateMrt = xmlMrtGraph;
	renderSystem->getOrCreateRenderPipeline("XmlGraphPBR", xmlPbrOptions);
	mpp::RenderPipelineOptions defaultOptions;
	defaultOptions.bloom = mBloomOptions;
	renderSystem->getOrCreateRenderPipeline("Default", defaultOptions);
	auto graphDefaultOptions = defaultOptions;
	graphDefaultOptions.mode = mpp::RenderPipelineMode::GraphLegacyForward;
	renderSystem->getOrCreateRenderPipeline("GraphDefault", graphDefaultOptions);

	std::string documentFoundationFailure;
	if (!mpp::app::runDocumentFoundationTests(&documentFoundationFailure))
	{
		throw std::runtime_error("Document foundation tests failed: " + documentFoundationFailure);
	}
	renderSystem->infoMessage("Document ID/snapshot/undo/path/atomic-save tests passed.");

	std::string diagnosticTestFailure;
	if (!mpp::runDiagnosticTests(&diagnosticTestFailure))
	{
		throw std::runtime_error("Structured diagnostic tests failed: " + diagnosticTestFailure);
	}
	renderSystem->infoMessage("Structured diagnostic contract tests passed.");

	std::string specializationTestFailure;
	if (!mpp::runPbrMaterialSpecializationTests(&specializationTestFailure))
	{
		throw std::runtime_error("PBR material specialization tests failed: " + specializationTestFailure);
	}
	renderSystem->infoMessage("PBR material specialization derivation/source tests passed.");

	std::string materialTestFailure;
	if (!mpp::resource_parsers::runMaterialResourceTests(resourceMgr, &materialTestFailure))
	{
		throw std::runtime_error("Material resource tests failed: " + materialTestFailure);
	}
	renderSystem->infoMessage("Material XML dispatch/single-definition binary/legacy migration tests passed.");

	auto importedGraphDocument = mpp::resource_parsers::PbrPipelineDocumentLoader::fromFile(options.resourceLocation + "PbrPipeline.rendergraph.xml");
	if (!importedGraphDocument.importedFromRenderGraph || !importedGraphDocument.graph || importedGraphDocument.graph->getPassCount() == 0)
		throw std::runtime_error("Standalone RenderGraph migration failed.");
	auto pipelineDocument = mpp::resource_parsers::PbrPipelineParser::fromFile(options.resourceLocation + "FullPbrPipeline.xml");
	if (pipelineDocument.validate().hasErrors()) throw std::runtime_error("Native PbrPipeline document validation failed.");
	auto localizedPipeline=pipelineDocument;if(!localizedPipeline.makeLocalCopy("DemoPreview::LinearClamp","Preview.CopiedLinearClamp")||localizedPipeline.localResources.size()!=pipelineDocument.localResources.size()+1||localizedPipeline.localResources.back().definition.getEntry("name").getValue()!="Preview.CopiedLinearClamp")throw std::runtime_error("PbrPipeline Make Local Copy failed.");
	auto pipelineRoundTrip = std::filesystem::temp_directory_path() / "mpp-pipeline-roundtrip.xml";
	mpp::resource_parsers::PbrPipelineSerializer::toFile(pipelineDocument, pipelineRoundTrip.string());
	auto roundTrippedPipeline = mpp::resource_parsers::PbrPipelineParser::fromFile(pipelineRoundTrip.string());
	{ std::ofstream invalid(pipelineRoundTrip,std::ios::trunc);invalid<<"<PbrPipeline><version>1</version><unknown>true</unknown></PbrPipeline>"; }
	bool rejectedUnknown=false;try{mpp::resource_parsers::PbrPipelineParser::fromFile(pipelineRoundTrip.string());}catch(std::exception const&){rejectedUnknown=true;}if(!rejectedUnknown)throw std::runtime_error("PbrPipeline parser accepted an unknown field.");
	std::filesystem::remove(pipelineRoundTrip);
	if (roundTrippedPipeline.name != pipelineDocument.name || roundTrippedPipeline.imports.size() != pipelineDocument.imports.size() || roundTrippedPipeline.localResources.size() != pipelineDocument.localResources.size() || roundTrippedPipeline.localResources.front().definition.getEntry("wrap").getValue() != pipelineDocument.localResources.front().definition.getEntry("wrap").getValue() || roundTrippedPipeline.extensions.size() != pipelineDocument.extensions.size() || roundTrippedPipeline.extensions.front().payload.getEntry("EditorMetadata").getEntry("category").getValue() != pipelineDocument.extensions.front().payload.getEntry("EditorMetadata").getEntry("category").getValue() || roundTrippedPipeline.previewOverrides.size() != pipelineDocument.previewOverrides.size() || roundTrippedPipeline.previewOverrides.front().values.getNumUniforms() != pipelineDocument.previewOverrides.front().values.getNumUniforms() || !roundTrippedPipeline.graph || roundTrippedPipeline.graph->getPassCount() != pipelineDocument.graph->getPassCount())
		throw std::runtime_error("Native PbrPipeline XML round trip failed.");
	auto pipelineResource = resourceMgr->declareResource("PBR.EditorPipelineTest", std::make_shared<mpp::resource_parsers::FilePbrPipelineStream>(resourceMgr, options.resourceLocation + "FullPbrPipeline.xml")).first;
	pipelineResource->load();
	pipelineResource->create();
	if (pipelineResource->getType() != "PbrPipeline") throw std::runtime_error("PbrPipeline resource stream created the wrong resource type.");
	auto localSampler=resourceMgr->getResource("PBR.EditorPipelineTest/Preview.LocalSampler");if(!localSampler)throw std::runtime_error("PbrPipeline local sampler child was not instantiated.");if(localSampler->getType()!="Sampler")throw std::runtime_error("PbrPipeline local sampler child has type '"+localSampler->getType()+"'.");if(!localSampler->isLoaded())throw std::runtime_error("PbrPipeline local sampler child was not loaded.");
	auto externalSampler=resourceMgr->getResource("PBR.EditorPipelineTest/DemoPreview::LinearClamp");if(!externalSampler||externalSampler->getType()!="Sampler"||!externalSampler->isLoaded())throw std::runtime_error("PbrPipeline read-only external sampler was not resolved and loaded.");
	renderSystem->infoMessage("Native PbrPipeline XML/resource parse/serialize/semantic validation passed.");
	auto sceneDocument = mpp::resource_parsers::SceneParser::fromFile(options.resourceLocation + pipelineDocument.previewScene);
	if (sceneDocument.validate().hasErrors()) throw std::runtime_error("Native Scene document validation failed.");
	auto sceneRoundTrip = std::filesystem::temp_directory_path() / "mpp-scene-roundtrip.xml";
	mpp::resource_parsers::SceneSerializer::toFile(sceneDocument, sceneRoundTrip.string());
	auto roundTrippedScene = mpp::resource_parsers::SceneParser::fromFile(sceneRoundTrip.string());
	{ std::ofstream invalid(sceneRoundTrip,std::ios::trunc);invalid<<"<Scene><version>1</version><unknown>true</unknown></Scene>"; }
	bool rejectedUnknownScene=false;try{mpp::resource_parsers::SceneParser::fromFile(sceneRoundTrip.string());}catch(std::exception const&){rejectedUnknownScene=true;}if(!rejectedUnknownScene)throw std::runtime_error("Scene parser accepted an unknown field.");
	std::filesystem::remove(sceneRoundTrip);
	if (roundTrippedScene.models.size() != sceneDocument.models.size() || roundTrippedScene.lights.size() != sceneDocument.lights.size() || roundTrippedScene.models.front().source != sceneDocument.models.front().source || roundTrippedScene.models.front().primitive.radius != sceneDocument.models.front().primitive.radius || roundTrippedScene.models.front().primitive.resolution != sceneDocument.models.front().primitive.resolution || roundTrippedScene.lights.back().type != sceneDocument.lights.back().type) throw std::runtime_error("Native Scene XML round trip failed.");
	auto sceneTemplateResource = resourceMgr->declareResource("PBR.EditorSceneTest", std::make_shared<mpp::resource_parsers::FileSceneStream>(resourceMgr, options.resourceLocation + pipelineDocument.previewScene)).first;
	sceneTemplateResource->load(); sceneTemplateResource->create();
	if (sceneTemplateResource->getType() != "SceneTemplate") throw std::runtime_error("SceneTemplate stream created the wrong resource type.");
	renderSystem->infoMessage("Native Scene XML/resource parse/serialize/semantic validation passed.");

	std::string graphGpuTestFailure;
	if (!mpp::runRenderGraphGpuTests(renderSystem, &graphGpuTestFailure))
	{
		throw std::runtime_error("Render graph GPU tests failed: " + graphGpuTestFailure);
	}
	renderSystem->infoMessage("Render graph GPU format/raster/state/stats/framebuffer/resize/MRT/MSAA/mip/alias/lifetime tests passed.");
}

void ModelScene::teardownImGui()
{
	delete mImGuiRenderer;
	mImGuiRenderer = nullptr;
}

void ModelScene::teardownImpl()
{
	teardownImGui();

	mGrid->release(this);
	mSphere->release(this);
	mCylinder->release(this);
	mBox->release(this);
	mTorus->release(this);
	mStatue->release(this);
	if (mPbrPreviewMaterial) mPbrPreviewMaterial->release(this);
}

mpp::CameraPtr ModelScene::createCamera(ProgramOptions const& options) const
{
	float aspectRatio = options.screenWidth / (float)options.screenHeight;

	//auto camera = new helper::FreeCamera(glm::vec3(0, 200, 750), 0.0f, 0.0f, 0.0f, 45.0f, aspectRatio);
	auto camera = new helper::FpsCamera(glm::vec3(0, 200, 750), 0.0f, 0.0f, 45.0f, aspectRatio);
	camera->setClipDistances(0.1f, 2000.0f);

	return shared_ptr<mpp::Camera>(camera);
}

string ModelScene::getRenderPipelineName() const
{
	return "PBR";
}

void ModelScene::toggle2dBatches(int batchId)
{
	if (batchId < kNum2dBatches)
	{
		auto visible = !m2dBatches[batchId].batch->isVisible();
		m2dBatches[batchId].batch->setVisible(visible);
	}
}

void ModelScene::toggleModels()
{
	getScene()->show3dModels(!getScene()->show3dModels());
}

void ModelScene::toggleModel(uint32_t index)
{
	for (uint32_t i = 0; i < 7; ++i)
	{
		auto params = mModels[i]->getParams();
		auto modelFlags = params->getModelFlags();

		if (i == index)
		{
			params->setModelFlags(modelFlags | mpp::ModelRenderParams::Flag_Visible);
		}
		else
		{
			params->setModelFlags(modelFlags & ~mpp::ModelRenderParams::Flag_Visible);
		}
	}
}

void ModelScene::handleInput(InputManager* inputMgr)
{
	mCameraOrbitInput = 0.0f;
	mCameraTargetVerticalInput = 0.0f;
	mLightMoveInput = glm::vec2(0.0f);

	const bool moveLight = inputMgr->keyDown(Key_LeftShift) || inputMgr->keyDown(Key_RightShift);
	if (moveLight)
	{
		if (inputMgr->keyDown(Key_LeftArrow)) mLightMoveInput.x -= 1.0f;
		if (inputMgr->keyDown(Key_RightArrow)) mLightMoveInput.x += 1.0f;
		if (inputMgr->keyDown(Key_UpArrow)) mLightMoveInput.y -= 1.0f;
		if (inputMgr->keyDown(Key_DownArrow)) mLightMoveInput.y += 1.0f;
	}
	else
	{
		if (inputMgr->keyDown(Key_LeftArrow)) mCameraOrbitInput -= 1.0f;
		if (inputMgr->keyDown(Key_RightArrow)) mCameraOrbitInput += 1.0f;
		if (inputMgr->keyDown(Key_UpArrow)) mCameraTargetVerticalInput += 1.0f;
		if (inputMgr->keyDown(Key_DownArrow)) mCameraTargetVerticalInput -= 1.0f;
	}
}

void ModelScene::renderUI(mpp::RenderSystem* renderSystem)
{
	auto drawList = ImGui::GetBackgroundDrawList();

	if (ImGui::Begin("DemoSuite"))
	{
		float gamma = renderSystem->getGamma();
		if (ImGui::SliderFloat("Gamma", &gamma, 1.0f, 4.0f))
		{
			renderSystem->setGamma(gamma);
		}

		int pipelineIndex = mSelectedPipeline == "PBR" ? 0 : (mSelectedPipeline == "GraphPBR" ? 1 : (mSelectedPipeline == "XmlGraphPBR" ? 2 : (mSelectedPipeline == "Default" ? 3 : 4)));
		if (ImGui::Combo("Render Pipeline", &pipelineIndex, "PBR (manual reference)\0PBR (hardcoded graph)\0PBR (XML graph)\0Default (manual reference)\0Default (render graph)\0"))
		{
			mSelectedPipeline = pipelineIndex == 0 ? "PBR" : (pipelineIndex == 1 ? "GraphPBR" : (pipelineIndex == 2 ? "XmlGraphPBR" : (pipelineIndex == 3 ? "Default" : "GraphDefault")));
		}
		ImGui::TextUnformatted("PBR: Cook-Torrance HDR; graph mode is explicit validation opt-in");
		auto pbrPipeline = renderSystem->getRenderPipeline("PBR");
		auto graphPbrPipeline = renderSystem->getRenderPipeline("GraphPBR");
		auto xmlGraphPbrPipeline = renderSystem->getRenderPipeline("XmlGraphPBR");
		bool graphPassesChanged = false;
		graphPassesChanged |= ImGui::Checkbox("Graph: Shadow Pass", &mGraphPassDebugOptions.shadow);
		graphPassesChanged |= ImGui::Checkbox("Graph: Scene Pass", &mGraphPassDebugOptions.scene);
		graphPassesChanged |= ImGui::Checkbox("Graph: Bloom Passes", &mGraphPassDebugOptions.bloom);
		graphPassesChanged |= ImGui::Checkbox("Graph: Presentation Pass", &mGraphPassDebugOptions.presentation);
		if (graphPassesChanged)
		{
			graphPbrPipeline->setGraphPassDebugOptions(mGraphPassDebugOptions);
			xmlGraphPbrPipeline->setGraphPassDebugOptions(mGraphPassDebugOptions);
			renderSystem->getRenderPipeline("GraphDefault")->setGraphPassDebugOptions(mGraphPassDebugOptions);
		}
		float exposure = pbrPipeline->getOptions().exposure;
		if (ImGui::SliderFloat("PBR Exposure", &exposure, 0.0f, 8.0f))
		{
			pbrPipeline->setExposure(exposure);
			graphPbrPipeline->setExposure(exposure);
			xmlGraphPbrPipeline->setExposure(exposure);
		}
		int toneMapOperator = pbrPipeline->getOptions().toneMapOperator == mpp::PbrToneMapOperator::Aces ? 1 : 0;
		if (ImGui::Combo("PBR Tone Map", &toneMapOperator, "Reinhard\0ACES\0"))
		{
			auto toneMap = toneMapOperator == 0 ? mpp::PbrToneMapOperator::Reinhard : mpp::PbrToneMapOperator::Aces;
			pbrPipeline->setToneMapOperator(toneMap);
			graphPbrPipeline->setToneMapOperator(toneMap);
			xmlGraphPbrPipeline->setToneMapOperator(toneMap);
		}

		bool bloomChanged = false;
		bloomChanged |= ImGui::Checkbox("Bloom Enabled", &mBloomOptions.enabled);
		bool mrtBloomAvailable = renderSystem->getCaps().maxDrawBuffers >= 2 && renderSystem->getCaps().maxColourAttachments >= 2;
		if (!mrtBloomAvailable) ImGui::BeginDisabled();
		bloomChanged |= ImGui::Checkbox("Graph PBR Emissive Bloom Mask (MRT)", &mBloomOptions.useMrtEmissiveMask);
		if (!mrtBloomAvailable)
		{
			ImGui::EndDisabled();
			ImGui::TextUnformatted("MRT bloom mask unavailable: falling back to threshold extract");
		}
		bloomChanged |= ImGui::SliderFloat("Bloom Threshold", &mBloomOptions.threshold, 0.0f, 4.0f, "%.2f");
		bloomChanged |= ImGui::SliderFloat("Bloom Intensity", &mBloomOptions.intensity, 0.0f, 2.0f, "%.2f");
		int bloomPasses = (int)mBloomOptions.blurPasses;
		if (ImGui::SliderInt("Bloom Blur Passes", &bloomPasses, 1, 4))
		{
			mBloomOptions.blurPasses = (uint32_t)bloomPasses;
			bloomChanged = true;
		}
		if (bloomChanged)
		{
			pbrPipeline->setBloomOptions(mBloomOptions);
			graphPbrPipeline->setBloomOptions(mBloomOptions);
			xmlGraphPbrPipeline->setBloomOptions(mBloomOptions);
			renderSystem->getRenderPipeline("Default")->setBloomOptions(mBloomOptions);
			renderSystem->getRenderPipeline("GraphDefault")->setBloomOptions(mBloomOptions);
		}

		bool shadowOptionsChanged = false;
		shadowOptionsChanged |= ImGui::Checkbox("Shadows Enabled", &mShadowOptions.enabled);
		int shadowFilter = mShadowOptions.filterMode == mpp::ShadowFilterMode::Pcf3x3 ? 1 : 0;
		if (ImGui::Combo("Shadow Filter", &shadowFilter, "Hard (1 tap)\0Soft (3x3 PCF)\0"))
		{
			mShadowOptions.filterMode = shadowFilter == 0 ? mpp::ShadowFilterMode::Hard : mpp::ShadowFilterMode::Pcf3x3;
			shadowOptionsChanged = true;
		}
		int shadowResolution = mShadowOptions.resolution == 512 ? 0 : (mShadowOptions.resolution == 2048 ? 2 : 1);
		if (ImGui::Combo("Shadow Resolution", &shadowResolution, "512\0 1024\0 2048\0"))
		{
			mShadowOptions.resolution = shadowResolution == 0 ? 512 : (shadowResolution == 1 ? 1024 : 2048);
			shadowOptionsChanged = true;
		}
		shadowOptionsChanged |= ImGui::SliderFloat("Shadow Extent", &mShadowOptions.orthoHalfWidth, 64.0f, 2000.0f, "%.0f");
		shadowOptionsChanged |= ImGui::SliderFloat("Shadow Constant Bias", &mShadowOptions.constantBias, 0.0f, 0.01f, "%.5f");
		shadowOptionsChanged |= ImGui::SliderFloat("Shadow Normal Bias", &mShadowOptions.normalBias, 0.0f, 0.02f, "%.5f");
		shadowOptionsChanged |= ImGui::SliderFloat("Shadow Filter Radius", &mShadowOptions.filterRadiusTexels, 0.25f, 3.0f, "%.2f");
		if (ImGui::SliderFloat3("Shadow Light Direction", &mShadowOptions.light.direction.x, -1.0f, 1.0f))
		{
			if (glm::dot(mShadowOptions.light.direction, mShadowOptions.light.direction) > 0.0001f)
			{
				mShadowOptions.light.direction = glm::normalize(mShadowOptions.light.direction);
				shadowOptionsChanged = true;
			}
		}
		if (shadowOptionsChanged)
		{
			renderSystem->configureShadowDomain("DemoSuite.MainDirectionalShadow", mShadowOptions);
		}

		if (ImGui::ColorEdit4("PBR Base Colour", &mPbrBaseColour.x))
		{
			mPbrStatueUniforms->updateUniform("PBR_BASE_COLOUR_FACTOR", mPbrBaseColour);
		}
		if (ImGui::SliderFloat("PBR Metallic", &mPbrMetallic, 0.0f, 1.0f))
		{
			mPbrStatueUniforms->updateUniform("PBR_METALLIC_FACTOR", mPbrMetallic);
		}
		if (ImGui::SliderFloat("PBR Roughness", &mPbrRoughness, 0.04f, 1.0f))
		{
			mPbrStatueUniforms->updateUniform("PBR_ROUGHNESS_FACTOR", mPbrRoughness);
		}
		if (ImGui::SliderFloat("PBR Light Intensity", &mPbrLightIntensity, 0.0f, 10.0f, "%.2f"))
		{
			// update() uploads the selected intensity into the dedicated PBR UBO.
		}
		if (ImGui::Combo("PBR Environment", &mPbrEnvironmentIndex, "Cool placeholder\0Warm placeholder\0"))
		{
			auto environmentMap = getResourceManager()->getResource(mPbrEnvironmentIndex == 0 ? "PBR.Preview.Environment" : "PBR.Preview.EnvironmentWarm");
			mPbrEnvironment->irradianceMap = environmentMap;
			mPbrEnvironment->prefilteredSpecularMap = environmentMap;
			mPbrEnvironment->backgroundMap = environmentMap;
			pbrPipeline->setPbrEnvironment(mPbrEnvironment);
		}
		ImGui::Text("Texture bindings: 9 dynamic samplers with shadows (limit: %u)", renderSystem->getCaps().maxFragmentTextureUnits);
		ImGui::TextUnformatted("Base/emissive: sRGB; normal, AO and metallic-roughness: linear");
		ImGui::Text("PBR lights: 1 / %zu; environment: selected precomputed placeholder", mpp::RenderSystem::getMaxPbrLights());
		ImGui::Text("Shadow domain: MainDirectionalShadow (%zux%zu, %s)", mShadowOptions.resolution, mShadowOptions.resolution,
			mShadowOptions.filterMode == mpp::ShadowFilterMode::Pcf3x3 ? "3x3 PCF" : "hard");
		ImGui::TextUnformatted("Arrows: orbit / move look target; Shift+Arrows: move light");
	}
	ImGui::End();
}

void ModelScene::updateImGui(float frameTime, mpp::RenderSystem* renderSystem)
{
	ImGuiIO& io = ImGui::GetIO();

	io.DeltaTime = frameTime;

	ImGui::NewFrame();

	renderUI(renderSystem);

	ImGui::EndFrame();
	ImGui::Render();

	mImGuiDataProvider->setDrawData(ImGui::GetDrawData());
}

void ModelScene::update(mpp::RenderSystem* renderSystem, float frameTime)
{
	mTotalTime += frameTime;

	updateImGui(frameTime, renderSystem);

	// Keep geometry stationary. Arrow keys orbit the camera or move its look-at
	// target; Shift+Arrow moves the shared directional/legacy light instead.
	mCameraOrbitAngle += mCameraOrbitInput * frameTime * 0.8f;
	mCameraOrbitTarget.y += mCameraTargetVerticalInput * frameTime * 120.0f;
	const float orbitRadius = 750.0f;
	const glm::vec3 orbitPosition(
		sinf(mCameraOrbitAngle) * orbitRadius,
		200.0f,
		cosf(mCameraOrbitAngle) * orbitRadius);
	getCamera()->setLookAt(orbitPosition, mCameraOrbitTarget);

	if (mLightMoveInput != glm::vec2(0.0f))
	{
		mLightPosition.x += mLightMoveInput.x * frameTime * 180.0f;
		mLightPosition.y += mLightMoveInput.y * frameTime * 180.0f;
		mShadowOptions.light.direction = glm::normalize(mShadowOptions.light.focusPoint - mLightPosition);
		renderSystem->configureShadowDomain("DemoSuite.MainDirectionalShadow", mShadowOptions);
	}

	// Scale cube
	/*
	m3dBatchDataProvider->update(frameTime);

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
	*/
	// Rotate batch box
	//auto& batchBoxModel = mModels[8];
	//batchBoxModel->rotateSelf(speed * frameTime, glm::normalize(glm::vec3(1, 1, 0)));
	//
	// Lighting
	mLightMarker->resetTransform();
	mLightMarker->translate(mLightPosition);
	mLightMarker->scale(glm::vec3(0.5f));
	renderSystem->setLight1Position(mLightPosition);
	mpp::PbrLight pbrLight;
	pbrLight.type = mpp::PbrLightType::Directional;
	pbrLight.direction = mShadowOptions.light.direction;
	pbrLight.colour = glm::vec3(1.0f);
	pbrLight.intensity = mPbrLightIntensity;
	pbrLight.range = 0.0f;
	renderSystem->setPbrLights({ pbrLight });

	// Update scene
	getScene()->update(frameTime);
}

void ModelScene::render(mpp::RenderSystem* renderSystem, World const& world, RenderOptions const& options)
{
	// Update 3d renderers
	m3dTestRenderer->update();
	m3dTestBufferRenderer->update();

	// Set render params
	for (auto model: mModels)
	{
		auto params = model->getParams();

		uint32_t flags = params->getModelFlags();

		if (options.wireframe)
		{
			flags |= ModelRenderParams::Flag_Wireframe;
		}
		else
		{
			flags &= ~ModelRenderParams::Flag_Wireframe;
		}

		params->setModelFlags(flags);
	}

	//getScene()->setClearColour(mpp::Colour::Grey50);
	renderSystem->renderScene(getScene(), getCamera(), glm::vec2(0.0f, 0.0f), mSelectedPipeline);

	// ImGui
	mImGuiRenderer->render(renderSystem);

	// Batch text
	for (int i = 0; i < kNum2dBatches; ++i)
	{
		auto const& batch = m2dBatches[i];

		if (batch.batch->isVisible())
		{
			renderSystem->renderText(batch.label, batch.x, (int)renderSystem->getWindowHeight() - batch.y, mpp::Colour::White);
		}
	}
}