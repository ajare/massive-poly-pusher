#include <cassert>

#include "mpp/program/Parser.h"

#include "mpp/ResourceManager.h"
#include "mpp/DefaultShaders.h"
#include "mpp/InternalFont.h"
#include "mpp/Program.h"
#include "mpp/Sampler.h"
#include "mpp/String.h"
#include "mpp/PostEffect.h"
#include "mpp/ProgrammaticTextureStream.h"
#include "mpp/ProgrammaticModelStream.h"
#include "mpp/ProgrammaticMaterialStream.h"
#include "mpp/ProgrammaticProgramStream.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{

	uint32_t ResourceManager::msSortableTextureId = 1;
	uint32_t ResourceManager::msSortableProgramId = 1;

	/*
	 * Constructor.
	 *
	 */
	ResourceManager::ResourceManager(RenderSystem* renderSystem)
		: mwRenderSystem(renderSystem)
	{
		// Pad sortable vectors
		for (uint32_t i = 0; i < msSortableTextureId; ++i)
		{
			mSortableTextures.push_back(ResourcePtr());
		}

		for (uint32_t i = 0; i < msSortableProgramId; ++i)
		{
			mSortablePrograms.push_back(ResourcePtr());
		}

		// Add factories
		mResourceFactories["Program"] = [this](string const& name, ResourceStreamPtr rStream)
		{
			return ResourcePtr(new Program(name, this->mwRenderSystem, this, rStream));
		};
		mResourceFactories["Sampler"] = [this](string const& name, ResourceStreamPtr rStream)
		{
			return ResourcePtr(new Sampler(name, this->mwRenderSystem, this, rStream));
		};
		mResourceFactories["Texture"] = [this](string const& name, ResourceStreamPtr rStream)
		{
			return ResourcePtr(new Texture(name, this->mwRenderSystem, this, rStream));
		};
		mResourceFactories["TextureAtlas"] = [this](string const& name, ResourceStreamPtr rStream)
		{
			return ResourcePtr(new TextureAtlas(name, this->mwRenderSystem, this, rStream));
		};
		mResourceFactories["RenderTexture"] = [this](string const& name, ResourceStreamPtr rStream)
		{
			return ResourcePtr(new RenderTexture(name, this->mwRenderSystem, this, rStream));
		};
		mResourceFactories["Material"] = [this](string const& name, ResourceStreamPtr rStream)
		{
			return ResourcePtr(new Material(name, this->mwRenderSystem, this, rStream));
		};
		mResourceFactories["Model"] = [this](string const& name, ResourceStreamPtr rStream)
		{
			return ResourcePtr(new Model(name, this->mwRenderSystem, this, rStream));
		};
		mResourceFactories["String"] = [this](string const& name, ResourceStreamPtr rStream)
		{
			return ResourcePtr(new String(name, this->mwRenderSystem, this, rStream));
		};
		mResourceFactories["PostEffect"] = [this](string const& name, ResourceStreamPtr rStream)
		{
			return ResourcePtr(new PostEffect(name, this->mwRenderSystem, this, rStream));
		};

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

			auto ps = new ProgrammaticProgramStream(this);
			ps->setParser(parser);
			declareResource("__mpp_p3d_tris_p3n3t2c4__", ResourceStreamPtr(ps))->load();
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

			auto ps = new ProgrammaticProgramStream(this);
			ps->setParser(parser);
			declareResource("__mpp_p2d_fullscreen__", ResourceStreamPtr(ps))->load();
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

			auto ps = new ProgrammaticProgramStream(this);
			ps->setParser(parser);
			ps->setAttribs({ "Points" });
			declareResource("__mpp_p2d_points_text__", ResourceStreamPtr(ps))->load();
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

			auto ps = new ProgrammaticProgramStream(this);
			ps->setParser(parser);
			declareResource("__mpp_p2d_tris_text__", ResourceStreamPtr(ps))->load();
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

			auto ps = new ProgrammaticProgramStream(this);
			ps->setParser(parser);
			ps->setAttribs({ "Points", "Colours" });
			declareResource("__mpp_p2d_points_text_coloured__", ResourceStreamPtr(ps))->load();
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

			auto ps = new ProgrammaticProgramStream(this);
			ps->setParser(parser);
			ps->setAttribs({ "Colours" });
			declareResource("__mpp_p2d_tris_text_coloured__", ResourceStreamPtr(ps))->load();
		}

		// Default texture
		auto blankStream = new ProgrammaticTextureStream(this);
		blankStream->setTarget(TextureTarget::Texture2D);
		blankStream->setData([](string const& id)
		{
			TextureData data;
			
			data.width = 2;
			data.height = 2;
			data.bitsPerPixel = 24;
			data.dataType = GL_UNSIGNED_BYTE;
			data.pixelFormat = GL_RGB;

			size_t dataSize = (data.width * data.height * data.bitsPerPixel / 8);
			
			data.data = new uint8_t[dataSize];
			memset(data.data, 255, dataSize);

			return data;
		});

		blankStream->setFiltering(TextureParams::MinFilter::Nearest, TextureParams::MagFilter::Nearest);
		declareResource("__mpp_tex_none__", ResourceStreamPtr(blankStream))->load();

		// Internal font texture
		auto ts = new ProgrammaticTextureStream(this);
		ts->setTarget(TextureTarget::Texture2D);
		ts->setData([](string const& id)
		{
			InternalFont internalFont;
			TextureData data;

			data.width = internalFont.getWidth();
			data.height = internalFont.getHeight();
			data.bitsPerPixel = 32;
			data.dataType = GL_UNSIGNED_BYTE;
			data.pixelFormat = GL_RGBA;

			size_t dataSize = (data.width * data.height * data.bitsPerPixel / 8);

			data.data = new uint8_t[dataSize];
			memcpy(data.data, (uint8_t const*)internalFont.getData(), dataSize);

			return data;
		});

		ts->setFiltering(TextureParams::MinFilter::Nearest, TextureParams::MagFilter::Nearest);

		declareResource("__mpp_tex_internalfont__", ResourceStreamPtr(ts))->load();

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
	void ResourceManager::logResourceEvent(Resource* resource, ResourceEvent event)
	{
		if (mLogResourceEvents && mwLogger)
		{
			string msg;
			switch (event)
			{
			case ResourceEvent::Create:
				msg = "CREATE-RES: "; break;
			case ResourceEvent::Destroy:
				msg = "DESTROY-RES: "; break;
			case ResourceEvent::Load:
				msg = "LOAD-RES: "; break;
			case ResourceEvent::Unload:
				msg = "UNLOAD-RES: "; break;
			case ResourceEvent::Reload:
				msg = "RELOAD-RES: "; break;
			}

			msg += resource->getName();

			mwLogger->message(msg);
		}
	}

	void ResourceManager::logResourceStreamEvent(ResourceStream* stream, ResourceStreamEvent event)
	{
		if (mLogResourceEvents)
		{
			string msg;
			switch (event)
			{
			case ResourceStreamEvent::Load:
				msg = "LOAD-STREAM: "; break;
			case ResourceStreamEvent::Unload:
				msg = "UNLOAD-STREAM: "; break;
			}

			msg += stream->getType();

			mwLogger->message(msg);
		}
	}
	*/

	void ResourceManager::setImageLoadFunction(ImageLoadFunction function)
	{
		mImageLoadFunction = function;
	}

	ImageLoadFunction ResourceManager::getImageLoadFunction()
	{
		return mImageLoadFunction;
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

	ResourcePtr ResourceManager::declareResource(string const& name, ResourceStreamPtr resourceStream, bool loadStream, uint32_t quality)
	{
		// Check name doen't exist
		if (mResources.find(name) != mResources.end())
		{
			THROW_MPP(
				utils::StringUtils::format("Resource '{}' already exists.", name),
				__LINE__, __FILE__, __func__);
		}

		if (loadStream)
		{
			resourceStream->load(quality);
		}

		string type = resourceStream->getType();

		// Check caching
		string fullSource{ "" };
		if (type == "Program")
		{
			// Must load to get source
			resourceStream->load(quality);

			auto programStream = dynamic_cast<ProgramStream*>(resourceStream.get());
			fullSource = programStream->getConcatenatedSource();

			auto createdProgram = mProgramCache.find(fullSource);
			if (createdProgram != mProgramCache.end())
			{
				mResources[name] = createdProgram->second;
				return createdProgram->second;
			}
		}

		// Create resource
		auto res = mResourceFactories[type](name, resourceStream);

		if (type == "Texture" || type == "TextureAtlas" || type == "RenderTexture")
		{
			// Set sort id
			uint64_t maxBits = min<uint64_t>(MPP_RENDER_SORT_TEXTURE0_BITS_SIZE, MPP_RENDER_SORT_TEXTURE1_BITS_SIZE);
			if (msSortableTextureId == (uint32_t)(1 << maxBits))
			{
				string errMsg = utils::StringUtils::format("Cannot create resource '{}'.  Limit reached!", name);
				THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
			}

			Texture* t = static_cast<Texture*>(res.get());

			t->setSortId(msSortableTextureId++);
			mSortableTextures.push_back(res);
		}
		else if (type == "Program")
		{
			// Caching
			if (msSortableProgramId == (1 << MPP_RENDER_SORT_PROGRAM_BITS_SIZE))
			{
				string errMsg = utils::StringUtils::format("Cannot create resource '{}'.  Limit reached!", name);
				THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
			}

			Program* p = static_cast<Program*>(res.get());

			p->setSortId(msSortableProgramId++);
			mSortablePrograms.push_back(res);

			// Add to cache
			mProgramCache[fullSource] = res;
		}

		mResources[name] = res;

		return res;
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
		for (size_t i = 0; i < spec.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto const& layout = spec.getVertexBufferAttributeLayout(i);
			for (size_t j = 0; j < layout.getNumAttributes(); ++j)
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
	ResourcePtr ResourceManager::getDefault2dProgram(mesh::MeshSpecification const& spec, uint32_t flags, bool load, string descriptor)
	{
		return getDefault2dProgram(VertexShader2dTemplate, FragmentShader2dTemplate, spec, flags, load, descriptor);
	}

	ResourcePtr ResourceManager::getDefault2dProgram(string const& defaultVertexShader, string const& defaultFragmentShader, mesh::MeshSpecification const& spec, uint32_t flags, bool load, string descriptor)
	{
		auto parser = make_shared<program::Parser>();

		parser->setMeshSpecification(spec);

		// Set defaults if empty
		string vShader = defaultVertexShader;
		if (vShader == "")
		{
			vShader = VertexShader2dTemplate;
		}

		string fShader = defaultFragmentShader;
		if (fShader == "")
		{
			fShader = FragmentShader2dTemplate;
		}

		parser->setVertexSource(vShader);
		parser->setFragmentSource(fShader);

		auto ps = new ProgrammaticProgramStream(this);
		ps->setParser(parser);
		ps->setAttribs(getProgramAttributes(spec, flags));

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

		if (flags & (MPP_PROGRAM_TAGS_DIFFUSE | MPP_PROGRAM_TAGS_ROTATION | MPP_PROGRAM_TAGS_ATLAS))
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

		auto res = declareResource(specName, ResourceStreamPtr(ps));

		if (load)
		{
			res->load();
		}

		return res;
	}

	ResourcePtr ResourceManager::getDefault3dProgram(mesh::MeshSpecification const& spec, uint32_t flags, bool load, string descriptor)
	{
		return getDefault3dProgram(VertexShader3dTemplate, FragmentShader3dTemplate, spec, flags, load, descriptor);
	}

	ResourcePtr ResourceManager::getDefault3dProgram(string const& defaultVertexShader, string const& defaultFragmentShader, mesh::MeshSpecification const& spec, uint32_t flags, bool load, string descriptor)
	{
		auto parser = make_shared<program::Parser>();

		parser->setMeshSpecification(spec);

		// Set defaults if empty
		string vShader = defaultVertexShader;
		if (vShader == "")
		{
			vShader = VertexShader3dTemplate;
		}
		
		string fShader = defaultFragmentShader;
		if (fShader == "")
		{
			fShader = FragmentShader3dTemplate;
		}

		parser->setVertexSource(vShader);
		parser->setFragmentSource(fShader);

		auto ps = new ProgrammaticProgramStream(this);
		ps->setParser(parser);
		ps->setAttribs(getProgramAttributes(spec, flags));

		// Generate name
		string specName = spec.getDescriptor("__mpp_p3d_");

		// Add texture units, lights diffuse, rotation.
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

		if (flags & MPP_PROGRAM_TAGS_DIFFUSE)
		{
			specName += "_";
		}
		if (flags & MPP_PROGRAM_TAGS_DIFFUSE)
		{
			specName += "d";
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
		specName += utils::StringUtils::toString(++mProgramIdCounter);

		specName += "__";

		auto res = declareResource(specName, ResourceStreamPtr(ps));

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
	ResourcePtr ResourceManager::getTextureBySortId(uint32_t id)
	{
		return mSortableTextures[id];
	}

	/*
	 * Get raw program resource from sort id.
	 *
	 */
	ResourcePtr ResourceManager::getProgramBySortId(uint32_t id)
	{
		return mSortablePrograms[id];
	}

	void ResourceManager::debugMessage(string const& message)
	{
		mwRenderSystem->debugMessage(message);
	}

	void ResourceManager::infoMessage(string const& message)
	{
		mwRenderSystem->infoMessage(message);
	}

	void ResourceManager::warnMessage(string const& message)
	{
		mwRenderSystem->warnMessage(message);
	}

	void ResourceManager::errorMessage(string const& message)
	{
		mwRenderSystem->errorMessage(message);
	}

}