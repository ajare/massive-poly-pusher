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

#include <mpp/MppModelStream.h>
#include <mpp/SphereModelStream.h>
#include <mpp/GridModelStream.h>
#include <mpp/ProgrammaticMaterialStream.h>
#include <mpp/TextureStream.h>

#include "ModelScene.h"
#include "Helper.h"

using namespace mpp;

ModelScene::ModelScene(mpp::ResourceManager* resourceMgr)
	: Scene(resourceMgr)
{
}

void ModelScene::createSharedTextures(ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	auto textureStream = new TextureStream(resourceMgr, options.resourceLocation + "marble_texture4662.jpg", loadImage, true);
	resourceMgr->createResource("Marble.Texture", ResourceStreamPtr(textureStream));
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

mpp::ResourcePtr ModelScene::createGridMaterial(mpp::mesh::MeshSpecification const& meshSpec)
{
	auto resourceMgr = getResourceManager();

	auto materialStream = new ProgrammaticMaterialStream(resourceMgr,
		false,
		meshSpec,
		"",
		false,
		"",
		false);

	materialStream->setTexture("TEX1", "Marble.Texture");

	auto res = resourceMgr->createResource("Grid.Material", ResourceStreamPtr(materialStream));
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
	attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::Float, true);

	meshSpec.setStorageType(mesh::VertexBufferStorageType::Static);
	meshSpec.setIndexedVertices(true);

	return meshSpec;
}

mpp::ResourcePtr ModelScene::createSphereMaterial(mpp::mesh::MeshSpecification const& meshSpec)
{
	auto resourceMgr = getResourceManager();

	auto materialStream = new ProgrammaticMaterialStream(resourceMgr,
		false,
		meshSpec,
		"",
		false,
		"",
		false); 
	
	materialStream->setTexture("TEX1", "Marble.Texture");

	auto res = resourceMgr->createResource("Sphere.Material", ResourceStreamPtr(materialStream));
	res->load();

	return res;
}

void ModelScene::setup(ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	createSharedTextures(options);

	// Textures are image files which are loaded with a helper function into a TextureStream, 
	// which takes the raw loaded data.  In this case, rgba.png is referenced by the statue
	// model, so needs to be explicitly loaded
	auto textureStream = new mpp::TextureStream(resourceMgr, options.resourceLocation + "rgba.png", loadImage, true);
	resourceMgr->createResource("rgba.png", ResourceStreamPtr(textureStream));

	// Load MppModel
	auto statueStream = new MppModelStream(resourceMgr, options.resourceLocation + "statue/statue.mppmodel");
	auto statue = resourceMgr->createResource("Model.Statue", ResourceStreamPtr(statueStream));
	statue->load();

	mModels.push_back({
		statue,
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(1.0f, 1.0f, 1.0f)
	});

	// Load Grid
	auto gridMeshSpec = createGridMeshSpecification();
	createGridMaterial(gridMeshSpec);

	auto gridStream = new GridModelStream(resourceMgr, gridMeshSpec, "Grid.Material", 256, 256, 8, 8);
	auto grid = resourceMgr->createResource("Model.Grid", ResourceStreamPtr(gridStream));
	grid->load();

	mModels.push_back({
		grid,
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(1.0f, 1.0f, 1.0f)
		});

	// Load Sphere
	auto sphereMeshSpec = createSphereMeshSpecification();
	createSphereMaterial(sphereMeshSpec);
	
	auto sphereStream = new SphereModelStream(resourceMgr, sphereMeshSpec, "Sphere.Material", 20, 2);
	auto sphere = resourceMgr->createResource("Model.Sphere", ResourceStreamPtr(sphereStream));
	sphere->load();

	mModels.push_back({
		sphere,
		glm::vec3(80.0f, 130.0f, 0.0f),
		glm::vec3(1.0f, 1.0f, 1.0f)
	});
}

void ModelScene::update(float frameTime)
{
	mTotalTime += frameTime;

	// Rotate sphere
	auto& sphereModel = mModels[2];

	float speed = 1.5f;
	float amt = speed * frameTime;

	float x = sphereModel.position.x;
	float z = sphereModel.position.z;
	
	sphereModel.position.x = x * cosf(amt) - z * sinf(amt);
	sphereModel.position.y = 130.0f + sinf(mTotalTime * 2.0f) * 25.0f;
	sphereModel.position.z = x * sinf(amt) + z * cosf(amt);
}

void ModelScene::render(mpp::RenderSystem* renderSystem, World const& world)
{
	// Set uniforms
	mpp::UniformCollection modelUniforms;
	if (!world.pointLights.empty())
	{
		modelUniforms.setUniform("light", world.pointLights.front());
	}

	for (auto const& model: mModels)
	{
		// Transform
		renderSystem->resetTransform();
		renderSystem->translateTransform3d(model.position);
		renderSystem->scaleTransform3d(model.scale);

		// Render
		auto mi = renderSystem->renderModelBatched((Model&)*model.model, true, &modelUniforms);
	}
}