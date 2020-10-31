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
#include <mpp/CylinderModelStream.h>
#include <mpp/BoxModelStream.h>
#include <mpp/ProgrammaticModelStream.h>
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

	textureStream = new TextureStream(resourceMgr, options.resourceLocation + "electbubbles.jpg", loadImage, true);
	resourceMgr->createResource("Electro.Texture", ResourceStreamPtr(textureStream));

	textureStream = new TextureStream(resourceMgr, options.resourceLocation + "test.png", loadImage, true);
	resourceMgr->createResource("Test.Texture", ResourceStreamPtr(textureStream));

	textureStream = new TextureStream(resourceMgr, options.resourceLocation + "donut.jpg", loadImage, true);
	resourceMgr->createResource("Doughnut.Texture", ResourceStreamPtr(textureStream));
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

	//materialStream->setTexture("TEX1", "Marble.Texture");

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
	
	//materialStream->setTexture("TEX1", "Electro.Texture");

	auto res = resourceMgr->createResource("Sphere.Material", ResourceStreamPtr(materialStream));
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

mpp::ResourcePtr ModelScene::createCylinderMaterial(mpp::mesh::MeshSpecification const& meshSpec)
{
	auto resourceMgr = getResourceManager();

	auto materialStream = new ProgrammaticMaterialStream(resourceMgr,
		false,
		meshSpec,
		"",
		false,
		"",
		false);

	//materialStream->setTexture("TEX1", "Marble.Texture");

	auto res = resourceMgr->createResource("Cylinder.Material", ResourceStreamPtr(materialStream));
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

mpp::ResourcePtr ModelScene::createBoxMaterial(mpp::mesh::MeshSpecification const& meshSpec)
{
	auto resourceMgr = getResourceManager();

	auto materialStream = new ProgrammaticMaterialStream(resourceMgr,
		false,
		meshSpec,
		"",
		false,
		"",
		false);

	//materialStream->setTexture("TEX1", "Test.Texture");

	auto res = resourceMgr->createResource("Box.Material", ResourceStreamPtr(materialStream));
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

mpp::ResourcePtr ModelScene::createTorusMaterial(mpp::mesh::MeshSpecification const& meshSpec)
{
	auto resourceMgr = getResourceManager();

	auto materialStream = new ProgrammaticMaterialStream(resourceMgr,
		false,
		meshSpec,
		"",
		false,
		"",
		false);

	//materialStream->setTexture("TEX1", "Doughnut.Texture");

	auto res = resourceMgr->createResource("Torus.Material", ResourceStreamPtr(materialStream));
	res->load();

	return res;
}

ResourcePtr ModelScene::createTorusModel()
{
	auto resourceMgr = getResourceManager();

	auto torusMeshSpec = createTorusMeshSpecification();
	createTorusMaterial(torusMeshSpec);

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

	auto torus = resourceMgr->createResource("Model.Torus", ResourceStreamPtr(torusStream));
	torus->load();

	return torus;
}

void ModelScene::setup(mpp::RenderSystem* renderSystem, ProgramOptions const& options)
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
		glm::vec3(1.0f, 1.0f, 1.0f),
		0.0f
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
		glm::vec3(1.0f, 1.0f, 1.0f),
		0.0f
		});

	// Load Sphere
	auto sphereMeshSpec = createSphereMeshSpecification();
	createSphereMaterial(sphereMeshSpec);
	
	auto sphereStream = new SphereModelStream(resourceMgr, sphereMeshSpec, "Sphere.Material", 40, 3);
	auto sphere = resourceMgr->createResource("Model.Sphere", ResourceStreamPtr(sphereStream));
	sphere->load();
	
	mModels.push_back({
		sphere,
		glm::vec3(-80.0f, 130.0f, 0.0f),
		glm::vec3(1.0f, 1.0f, 1.0f),
		0.0f
	});

	// Load Cylinder
	auto cylinderMeshSpec = createCylinderMeshSpecification();
	createCylinderMaterial(cylinderMeshSpec);

	auto cylinderStream = new CylinderModelStream(resourceMgr, cylinderMeshSpec, "Cylinder.Material", 80, 24, 24, 16);
	auto cylinder = resourceMgr->createResource("Model.Cylinder", ResourceStreamPtr(cylinderStream));
	cylinder->load();

	mModels.push_back({
		cylinder,
		glm::vec3(96.0f, 40.0f, 96.0f),
		glm::vec3(1.0f, 1.0f, 1.0f),
		0.0f
		});

	mModels.push_back({
		cylinder,
		glm::vec3(-96.0f, 40.0f, 96.0f),
		glm::vec3(1.0f, 1.0f, 1.0f),
		0.0f
		});

	// Load Box
	auto boxMeshSpec = createBoxMeshSpecification();
	createBoxMaterial(boxMeshSpec);

	auto boxStream = new BoxModelStream(resourceMgr, cylinderMeshSpec, "Box.Material", 32, 32, 32);
	auto box = resourceMgr->createResource("Model.Box", ResourceStreamPtr(boxStream));
	box->load();

	mModels.push_back({
		box,
		glm::vec3(96.0f, 108.0f, 96.0f),
		glm::vec3(1.0f, 1.0f, 1.0f),
		0.0f
		});

	mModels.push_back({
		box,
		glm::vec3(-96.0f, 108, 96.0f),
		glm::vec3(1.0f, 1.0f, 1.0f),
		0.0f
		});

	// Load torus
	auto torus = createTorusModel();

	mModels.push_back({
		torus,
		glm::vec3(0.0f, 280.0f, 0.0f),
		glm::vec3(1.0f, 1.0f, 1.0f),
		0.0f
		});

	// Lighting
	renderSystem->setAmbientColour(Colour::Red);
	renderSystem->setLightCount(1);
	renderSystem->setLight1Position(glm::vec3(256, 0, 256));
	renderSystem->setLight1Colour(Colour::White);
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

	sphereModel.angle += frameTime * 50;

	// Rotate boxes
	mModels[5].angle += frameTime * 50;
	mModels[6].angle += frameTime * 50;

	// Rotate torus
	mModels[7].angle = sin(mTotalTime) * 25.0f;
}

void ModelScene::render(mpp::RenderSystem* renderSystem, glm::vec3 const& viewPos, glm::vec3 const& viewDir, World const& world, RenderOptions const& options)
{
	for (size_t i = 0; i < mModels.size(); ++i)
	{
		auto const& model = mModels[i];

		// Transform
		renderSystem->resetTransform();
		renderSystem->translateTransform3d(model.position);
		renderSystem->scaleTransform3d(model.scale);

		switch (i)
		{
		case 0: // Statue
		case 1: // Grid
		case 2: // Sphere
		case 3: // Cylinder
		case 4: // Cylinder
			renderSystem->rotateTransform3d(model.angle, glm::vec3(0.0f, 1.0f, 0.0f));
			break;
		case 5: // Box
			renderSystem->rotateTransform3d(model.angle, glm::vec3(-0.707107f, 0.707107f, 0.0f));
			break;
		case 6: // Box
			renderSystem->rotateTransform3d(model.angle, glm::vec3(0.707107f, 0.0f, 0.707107f));
			break;
		case 7: // Torus
			renderSystem->rotateTransform3d(model.angle, glm::vec3(0.707107f, 0.0f, 0.707107f));
		default:
			break;
		}

		// Render
		auto mi = renderSystem->renderModelBatched((Model&)*model.model, true, viewPos);

		if (options.wireframe)
		{
			mi->setWireframe(true);
		}
	}
}