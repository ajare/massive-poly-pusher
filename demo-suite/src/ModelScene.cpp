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

#include "ModelScene.h"
#include "Helper.h"

using namespace mpp;

ModelScene::ModelScene(mpp::ResourceManager* resourceMgr)
	: Scene(resourceMgr)
{
}

void ModelScene::setup(ProgramOptions const& options)
{
	auto resourceMgr = getResourceManager();

	// Textures are image files which are loaded with a helper function into a TextureStream, 
	// which takes the raw loaded data.  In this case, rgba.png is referenced by the statue
	// model, so needs to be explicitly loaded
	auto textureStream = new mpp::TextureStream(resourceMgr, options.resourceLocation + "rgba.png", loadImage, true);
	resourceMgr->createResource("rgba.png", ResourceStreamPtr(textureStream));

	// Load MppModel
	auto statueStream = new MppModelStream(resourceMgr, options.resourceLocation + "statue/statue.mppmodel");
	mStatue = resourceMgr->createResource("Model.Statue", ResourceStreamPtr(statueStream));
	mStatue->load();

	// Set model to render
	mModelId = ModelId::Statue;
}

void ModelScene::update(float frameTime)
{
}

void ModelScene::render(mpp::RenderSystem* renderSystem, World const& world)
{
	// Transform
	renderSystem->resetTransform();

	switch (mModelId)
	{
	case ModelId::None:
		break;

	case ModelId::Statue:
		renderSystem->translateTransform3d(glm::vec3(0.0f, -170.0f, -80.0f));
		renderSystem->scaleTransform3d(glm::vec3(1.0f, 1.0f, 1.0f));
		break;
	};

	// Set uniforms.
	mpp::UniformCollection modelUniforms;
	if (!world.pointLights.empty())
	{
		modelUniforms.setUniform("light", world.pointLights.front());
	}

	// Render model
	auto mi = renderSystem->renderModelBatched((Model&)*mStatue, true, &modelUniforms);
}