#include <algorithm>
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
	ResourceManager::ResourceManager(RenderSystem* renderSystem, Logger* logger)
		: ResourceWrangler("ResourceManager")
		, mwRenderSystem(renderSystem)
		, mLogger(logger)
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
	}

	ResourceManager::~ResourceManager()
	{
		mLogger->info("Clearing up resources.");

		// Check integrity first
		for (auto const& kvp : mResources)
		{
			auto res = kvp.second;
			validateForRemoval(res);
		}

		// Clean up
		for (auto const& kvp : mResources)
		{
			auto res = kvp.second;
			res->destroy();
		}
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
	* Check a resource's integrity before removing it.
	*
	*/
	bool ResourceManager::validateForRemoval(ResourcePtr resource)
	{
		bool valid{ true };

		if (resource->getRefCount() != 0)
		{
			mLogger->warn("Resource '" + resource->getName() + "' is still referenced.");
			valid = false;
		}

		if (resource->getDependingObjectCount() != 0)
		{
			mLogger->warn("Resource '" + resource->getName() + "' has objects which have not yet released it.");

			auto const& deps = resource->getDependingResources();
			for (auto dep : deps)
			{
				mLogger->warn("Resource '" + resource->getName() + "' is awaiting release from '" + dep->getWranglerName() + "'.");
			}

			valid = false;
		}

		if (resource->getDependentResourceCount() != 0)
		{
			mLogger->warn("Resource '" + resource->getName() + "' has dependent resources it has not released yet.");

			auto const& deps = resource->getDependentResources();
			for (auto dep : deps)
			{
				mLogger->warn("Resource '" + resource->getName() + "' is yet to release '" + dep->getName() + "'.");
			}

			valid = false;
		}

		return valid;
	}

	/*
	* Remove a resource completely from the system.
	*
	*/
	void ResourceManager::deleteResource(string const& name)
	{
		auto resource = getResource(name);
		if (!validateForRemoval(resource))
		{
			throw MppException("Could not remove resource '" + name + "' from system as it still has references.");
		}

		resource->destroy();
		removeResource(resource);
	}

	void ResourceManager::insertResource(ResourcePtr resource, ResourceStreamPtr resourceStream)
	{
		auto const& name = resource->getName();
		auto const& type = resource->getType();

		if (type == "Texture" || type == "RenderTexture")
		{
			// Set sort id
			uint64_t maxBits = min<uint64_t>(MPP_RENDER_SORT_TEXTURE0_BITS_SIZE, MPP_RENDER_SORT_TEXTURE1_BITS_SIZE);
			if (msSortableTextureId == (uint32_t)(1 << maxBits))
			{
				string errMsg = utils::StringUtils::format("Cannot create resource '{}'.  Limit reached!", name);
				THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
			}

			Texture* t = static_cast<Texture*>(resource.get());

			t->setSortId(msSortableTextureId++);
			mSortableTextures.push_back(resource);
		}
		else if (type == "Program")
		{
			// Caching
			if (msSortableProgramId == (1 << MPP_RENDER_SORT_PROGRAM_BITS_SIZE))
			{
				string errMsg = utils::StringUtils::format("Cannot create resource '{}'.  Limit reached!", name);
				THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
			}

			Program* p = static_cast<Program*>(resource.get());

			p->setSortId(msSortableProgramId++);
			mSortablePrograms.push_back(resource);

			// Add to cache
			auto programStream = dynamic_cast<ProgramStream*>(resourceStream.get());
			auto sourceCode = programStream->getConcatenatedSource();
			mProgramCache[sourceCode] = resource;
		}

		mResources[name] = resource;
	}

	void ResourceManager::removeResource(ResourcePtr resource)
	{
		auto const& name = resource->getName();

		// If we have any other references, this will not yet delete the resource, but it will at least remove it from the system
		mResources.erase(name);

		auto type = resource->getType();
		if (type == "Texture" || type == "RenderTexture")
		{
			mSortableTextures.erase(remove(mSortableTextures.begin(), mSortableTextures.end(), resource), mSortableTextures.end());
		}
		if (type == "Program")
		{
			mSortablePrograms.erase(remove(mSortablePrograms.begin(), mSortablePrograms.end(), resource), mSortablePrograms.end());

			// Remove from program cache: this is a bit awkward
			auto it = find_if(mProgramCache.begin(), mProgramCache.end(), [resource](auto const& kvp)
			{
				auto const&[key, value] = kvp;
				return value == resource;
			});

			mProgramCache.erase(it);
		}
	}

	/*
	 * Create all resources.
	 *
	 */
	void ResourceManager::createAllResources()
	{
		for (auto it: mResources)
		{
			auto res = it.second;
			res->create();
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
			auto res = it.second;
			res->load();
		}
	}

	/*
	 * Unload all resources with no references.
	 *
	 */
	void ResourceManager::unloadAllUnreferencedResources()
	{
		for (auto it : mResources)
		{
			auto res = it.second;
			if (!res->isReferenced())
			{
				res->unload();
			}
		}
	}

	/*
	 * Destroy all resources with no references.
	 *
	 */
	void ResourceManager::destroyAllUnreferencedResources()
	{
		for (auto it : mResources)
		{
			auto res = it.second;
			if (!res->isReferenced())
			{
				res->destroy();
			}
		}
	}

	pair<ResourcePtr, bool> ResourceManager::declareResource(string const& name, ResourceStreamPtr resourceStream, bool loadStream, uint32_t quality)
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
		if (type == "Program")
		{
			// Must load to get source
			resourceStream->load(quality);

			auto programStream = dynamic_cast<ProgramStream*>(resourceStream.get());
			auto sourceCode = programStream->getConcatenatedSource();
			auto createdProgram = mProgramCache.find(sourceCode);

			if (createdProgram != mProgramCache.end())
			{
				return make_pair(createdProgram->second, false);
			}
		}

		// Create resource and insert
		auto resource = mResourceFactories[type](name, resourceStream);
		insertResource(resource, resourceStream);

		return make_pair(resource, true);
	}

	ResourcePtr ResourceManager::acquireResource(ResourceWrangler* wrangler, string const& name)
	{
		auto res = getResource(name);
		res->acquire(wrangler);
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

		if (flags & MPP_PROGRAM_TAGS_TEXTURE)
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
		if (flags & MPP_PROGRAM_TAGS_TEXTURE)
		{
			specName += "_s";
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
		specName += utils::StringUtils::toString(++mProgramIdCounter);

		specName += "__";

		auto res = declareResource(specName, ResourceStreamPtr(ps)).first;

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
		if (flags & MPP_PROGRAM_TAGS_TEXTURE)
		{
			specName += "_s";
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

		auto res = declareResource(specName, ResourceStreamPtr(ps)).first;

		if (load)
		{
			res->load();
		}

		return res;
	}

	vector<ResourcePtr> ResourceManager::getAllReferencedResources() const
	{
		vector<ResourcePtr> resources;

		for (auto item : mResources)
		{
			auto resource = item.second;
			if (resource->isReferenced())
			{
				resources.push_back(resource);
			}
		}

		return resources;
	}

	vector<ResourcePtr> ResourceManager::getAllUnreferencedResources() const
	{
		vector<ResourcePtr> resources;

		for (auto item : mResources)
		{
			auto resource = item.second;
			if (!resource->isReferenced())
			{
				resources.push_back(resource);
			}
		}

		return resources;
	}

	void ResourceManager::getResourceCounts(uint32_t& numResources, uint32_t& numDeclared, uint32_t& numCreated, uint32_t& numLoaded) const
	{
		numResources = mResources.size();
		numDeclared = 0;
		numCreated = 0;
		numLoaded = 0;

		for (auto item : mResources)
		{
			auto resource = item.second;
			if (resource->isLoaded())
			{
				numLoaded++;
			}
			else if (resource->isCreated())
			{
				numCreated++;
			}
			else
			{
				numDeclared++;
			}
		}
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

	/*
	 * Write resource status to CSV.
	 *
	 */
	void ResourceManager::dumpResources(string const& filepath)
	{
		ofstream fp;
		fp.open(filepath);

		fp << "Name,Type,Id,Ref_Count,State,GL_States,GL_Count\n";

		for (auto kvp : mResources)
		{
			auto resource = kvp.second;

			string state;
			if (resource->isLoaded())
			{
				state = "Loaded";
			}
			else if (resource->isCreated())
			{
				state = "Created";
			}
			else
			{
				state = "Declared";
			}

			fp << resource->getName() << "," 
				<< resource->getType() << "," 
				<< resource->getId() << "," 
				<< resource->getRefCount() << ","
				<< state << ","
				<< resource->getLiveIdCount() << ","
				<< resource->getIdCount() << "\n";
		}

		fp.close();
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