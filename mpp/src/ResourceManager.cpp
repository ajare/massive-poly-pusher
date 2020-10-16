#include <cassert>

#include "mpp/program/Parser.h"

#include "mpp/ResourceManager.h"
#include "mpp/DefaultShaders.h"
#include "mpp/InternalFont.h"
#include "mpp/Program.h"
#include "mpp/TextureStream.h"
#include "mpp/ProgrammaticModelStream.h"
#include "mpp/ProgrammaticMaterialStream.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{

	uint32 ResourceManager::msSortableTextureId = 1;
	uint32 ResourceManager::msSortableProgramId = 1;

	/*
	 * Constructor.
	 *
	 */
	ResourceManager::ResourceManager(RenderSystem* renderSystem)
		: mwRenderSystem(renderSystem)
	{
		// Pad sortable vectors
		for (uint32 i = 0; i < msSortableTextureId; ++i)
		{
			mSortableTextures.push_back(nullptr);
		}

		for (uint32 i = 0; i < msSortableProgramId; ++i)
		{
			mSortablePrograms.push_back(nullptr);
		}

		//
		// Create built-in resources
		//

		// Default 3d program
		{
			mesh::MeshSpecification meshSpec;
			
			auto layout = meshSpec.createVertexBufferAttributeLayout(false);
			layout->createAttribute(mesh::Vertex::Component::Position3, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::Normal3, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::UnsignedByte, true);

			auto parser = make_shared<program::Parser>();

			parser->setMeshSpecification(meshSpec);
			parser->setVertexSource(VertexShader3dTemplate);
			parser->setFragmentSource(FragmentShader3dTemplate);

			auto ps = new ProgramStream(this, parser, {});
			createResource<Program>("__mpp_p3d_tris_p3n3t2c4__", ResourceStreamPtr(ps))->load();
		}

		// 2d fullscreen program
		{
			mesh::MeshSpecification meshSpec;

			auto layout = meshSpec.createVertexBufferAttributeLayout(false);
			layout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);

			auto parser = make_shared<program::Parser>();

			parser->setMeshSpecification(meshSpec);
			parser->setVertexSource(VertexShaderFullscreenTemplate);
			parser->setFragmentSource(FragmentShaderFullscreenTemplate);

			auto ps = new ProgramStream(this, parser, {});
			createResource<Program>("__mpp_p2d_fullscreen__", ResourceStreamPtr(ps))->load();
		}

		// Internal text programs
		{
			mesh::MeshSpecification meshSpec;

			auto layout = meshSpec.createVertexBufferAttributeLayout(false);
			layout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::TexCoord4, mesh::Vertex::DataType::Float, false);

			auto parser = make_shared<program::Parser>();

			parser->setMeshSpecification(meshSpec);
			parser->setVertexSource(VertexShaderTextTemplate);
			parser->setFragmentSource(FragmentShaderTextTemplate);

			auto ps = new ProgramStream(this, parser, {"Points"});
			createResource<Program>("__mpp_p2d_points_text__", ResourceStreamPtr(ps))->load();
		}
		{
			mesh::MeshSpecification meshSpec;

			auto layout = meshSpec.createVertexBufferAttributeLayout(false);
			layout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);

			auto parser = make_shared<program::Parser>();

			parser->setMeshSpecification(meshSpec);
			parser->setVertexSource(VertexShaderTextTemplate);
			parser->setFragmentSource(FragmentShaderTextTemplate);

			auto ps = new ProgramStream(this, parser, {});
			createResource<Program>("__mpp_p2d_tris_text__", ResourceStreamPtr(ps))->load();
		}
		{
			mesh::MeshSpecification meshSpec;

			auto layout = meshSpec.createVertexBufferAttributeLayout(false);
			layout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::TexCoord4, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::UnsignedByte, true);

			auto parser = make_shared<program::Parser>();

			parser->setMeshSpecification(meshSpec);
			parser->setVertexSource(VertexShaderTextTemplate);
			parser->setFragmentSource(FragmentShaderTextTemplate);

			auto ps = new ProgramStream(this, parser, { "Points", "Colours" });
			createResource<Program>("__mpp_p2d_points_text_coloured__", ResourceStreamPtr(ps))->load();
		}
		{
			mesh::MeshSpecification meshSpec;

			auto layout = meshSpec.createVertexBufferAttributeLayout(false);
			layout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::UnsignedByte, true);

			auto parser = make_shared<program::Parser>();

			parser->setMeshSpecification(meshSpec);
			parser->setVertexSource(VertexShaderTextTemplate);
			parser->setFragmentSource(FragmentShaderTextTemplate);

			auto ps = new ProgramStream(this, parser, { "Colours" });
			createResource<Program>("__mpp_p2d_tris_text_coloured__", ResourceStreamPtr(ps))->load();
		}

		// Default texture
		vector<uint8> whiteData(16, 255);

		TextureStream* blankStream = new TextureStream(this, &(whiteData[0]), 2, 2, 32, false);
		createResource<Texture>("__mpp_tex_none__", ResourceStreamPtr(blankStream))->load();

		// Internal font texture
		InternalFont internalFont;

		TextureStream* ts = new TextureStream(
			this,
			(uint8 const*)internalFont.getData(),
			internalFont.getWidth(),
			internalFont.getHeight(),
			32,
			false);

		createResource<Texture>("__mpp_tex_internalfont__", ResourceStreamPtr(ts))->load();

		// 2D materials
		// ...

		// 3D materials
		// ...
	}

	/*
	 * Destructor.
	 *
	 */
	ResourceManager::~ResourceManager()
	{
		destroyAllResources();
	}

	/*
	 * Create all resources.
	 *
	 */
	void ResourceManager::createAllResources()
	{
		for (auto it: mResources)
		{
			it.second->create();
		}
	}

	/*
	 * Destroy all resources.
	 *
	 */
	void ResourceManager::destroyAllResources()
	{
		for (auto it: mResources)
		{
			it.second->destroy();
		}
	}

	/*
	 * Load all resources.
	 *
	 */
	void ResourceManager::loadAllResources()
	{
		for (auto it: mResources)
		{
			it.second->load();
		}
	}

	/*
	 * Unload all resources.
	 *
	 */
	void ResourceManager::unloadAllResources()
	{
		for (auto it: mResources)
		{
			it.second->unload();
		}
	}

	/*
	 * Get named resource.
	 *
	 */
	ResourcePtr ResourceManager::getResource(string const& name, bool nullIfNotFound)
	{
		if (mResources.find(name) == mResources.end())
		{
			if (nullIfNotFound)
			{
				return ResourcePtr();
			}
			else
			{
				THROW_MPP(utils::StringUtils::format("Resource '{}' not found.", name), __LINE__, __FILE__, __func__);
			}
		}

		return mResources[name];
	}

	set<std::string> ResourceManager::getProgramAttributes(mesh::MeshSpecification const& spec, uint32_t flags) const
	{
		set<string> attribs;

		// Mesh specification
		for (int i = 0; i < spec.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto const& layout = spec.getVertexBufferAttributeLayout(i);
			for (int j = 0; j < layout.getNumAttributes(); ++j)
			{
				auto const& attrib = layout.getAttribute(j);

				switch (attrib.component)
				{
				case mesh::Vertex::Component::Position2:
					attribs.insert("Position2");
					attribs.insert("Position");
					break;
				case mesh::Vertex::Component::Position3:
					attribs.insert("Position3");
					attribs.insert("Position");
					break;
				case mesh::Vertex::Component::Position4:
					attribs.insert("Position4");
					attribs.insert("Position");
					break;
				case mesh::Vertex::Component::Normal3:
					attribs.insert("Normal3");
					attribs.insert("Normal");
					break;
				case mesh::Vertex::Component::Normal4:
					attribs.insert("Normal4");
					attribs.insert("Normal");
					break;
				case mesh::Vertex::Component::TexCoord2:
					attribs.insert("TexCoords2");
					attribs.insert("TexCoords");
					break;
				case mesh::Vertex::Component::TexCoord3:
					attribs.insert("TexCoords3");
					attribs.insert("TexCoords");
					break;
				case mesh::Vertex::Component::TexCoord4:
					attribs.insert("TexCoords4");
					attribs.insert("TexCoords");
					break;
				case mesh::Vertex::Component::Colour1:
					attribs.insert("Colour1");
					attribs.insert("Alpha");
					attribs.insert("Colour");
					break;
				case mesh::Vertex::Component::Colour3:
					attribs.insert("Colour3");
					attribs.insert("RGB");
					attribs.insert("Colour");
					break;
				case mesh::Vertex::Component::Colour4:
					attribs.insert("Colour4");
					attribs.insert("RGBA");
					attribs.insert("Colour");
					break;
				default:
					break;
				}
			}
		}

		// Flags
		if (flags & MPP_PROGRAM_TAGS_PRIM_POINTS)
		{
			attribs.insert("Points");
		}

		if (flags & MPP_PROGRAM_TAGS_PRIM_LINES)
		{
			attribs.insert("Lines");
		}

		if (flags & MPP_PROGRAM_TAGS_PRIM_TRIANGLES)
		{
			attribs.insert("Triangles");
		}

		if (flags & (MPP_PROGRAM_TAGS_TEXTURE1 | MPP_PROGRAM_TAGS_TEXTURE2 | MPP_PROGRAM_TAGS_TEXTURE3 | MPP_PROGRAM_TAGS_TEXTURE4))
		{
			attribs.insert("Texture");
		}

		// See MeshSpecification::getHashCode()
		if (flags & 0x300) // Colour bits
		{
			attribs.insert("Colours");
		}

		if (flags & MPP_PROGRAM_TAGS_ROTATION)
		{
			attribs.insert("Rotation");
		}

		if (flags & MPP_PROGRAM_TAGS_DIFFUSE)
		{
			attribs.insert("Diffuse");
		}

		if (flags & MPP_PROGRAM_TAGS_ATLAS)
		{
			attribs.insert("Atlas");
		}

		return attribs;
	}

	/*
	 * Gets (or creates if not existing) a 2d program based on the given spec and flags.
	 *
	 */
	ResourcePtr ResourceManager::getDefault2dProgram(mesh::MeshSpecification const& spec, uint32 flags, bool load, string descriptor)
	{
		return getDefault2dProgram(VertexShader2dTemplate, FragmentShader2dTemplate, spec, flags, load, descriptor);
	}

	ResourcePtr ResourceManager::getDefault2dProgram(string const& defaultVertexShader, string const& defaultFragmentShader, mesh::MeshSpecification const& spec, uint32 flags, bool load, string descriptor)
	{
		auto parser = make_shared<program::Parser>();

		parser->setMeshSpecification(spec);
		parser->setVertexSource(defaultVertexShader);
		parser->setFragmentSource(defaultFragmentShader);

		auto ps = new ProgramStream(this, parser, getProgramAttributes(spec, flags));

		// Generate name
		string specName = spec.getDescriptor("__mpp_p2d_");

		// Add texture units, diffuse, rotation.
		if (flags & (MPP_PROGRAM_TAGS_TEXTURE1 | MPP_PROGRAM_TAGS_TEXTURE2 | MPP_PROGRAM_TAGS_TEXTURE3 | MPP_PROGRAM_TAGS_TEXTURE4))
		{
			specName += "_s";
		}
		if (flags & MPP_PROGRAM_TAGS_TEXTURE1)
		{
			specName += "1";
		}
		if (flags & MPP_PROGRAM_TAGS_TEXTURE2)
		{
			specName += "2";
		}
		if (flags & MPP_PROGRAM_TAGS_TEXTURE3)
		{
			specName += "3";
		}
		if (flags & MPP_PROGRAM_TAGS_TEXTURE4)
		{
			specName += "4";
		}

		if (flags & (MPP_PROGRAM_TAGS_DIFFUSE | MPP_PROGRAM_TAGS_ROTATION))
		{
			specName += "_";
		}
		if (flags & MPP_PROGRAM_TAGS_DIFFUSE)
		{
			specName += "d";
		}
		if (flags & MPP_PROGRAM_TAGS_ROTATION)
		{
			specName += "r";
		}
		if (flags & MPP_PROGRAM_TAGS_ATLAS)
		{
			specName += "a";
		}

		if (descriptor != "")
		{
			specName += "_" + descriptor;
		}

		// Append number of programs on, as this spec name will not be unique (eg, it does not differentiate
		// between attribute type).
		specName += "_";
		specName += utils::StringUtils::toString(mProgramCache.size() + 1);

		specName += "__";

		auto res = createResource<Program>(specName, ResourceStreamPtr(ps));

		if (load)
		{
			res->load();
		}

		return res;
	}

	/*
	 * Get raw texture resource from sort id.
	 *
	 */
	Texture* ResourceManager::getTextureBySortId(uint32 id)
	{
		return mSortableTextures[id];
	}

	/*
	 * Get raw program resource from sort id.
	 *
	 */
	Program* ResourceManager::getProgramBySortId(uint32 id)
	{
		return mSortablePrograms[id];
	}

}