#pragma once

#include <map>
#include <algorithm>
#include <string>
#include <sstream>
#include <vector>
#include <iterator>
#include <cctype>
#include <locale>
#include <functional>

#include "mpp/Config.h"
#include "mpp/Resource.h"
#include "mpp/ResourceStream.h"
#include "mpp/RenderSystem.h"
#include "mpp/MeshSortFlags.h"
#include "mpp/MppException.h"
#include "mpp/TextureAtlas.h"
#include "mpp/ProgramStream.h"
#include "mpp/TextureStream.h"

#include "mpp/mesh/MeshSpecification.h"

namespace mpp
{

	typedef std::function<ResourcePtr(std::string const&, ResourceStreamPtr)> ResourceFactory;

	class _MPPAPI ResourceManager
	{
		std::map<std::string, ResourceFactory> mResourceFactories;

		std::map<std::string, ResourcePtr> mResources;

		std::map<std::string, ResourcePtr> mProgramCache;

		RenderSystem* mwRenderSystem;

		static uint32 msSortableTextureId;

		static uint32 msSortableProgramId;

		std::vector<Texture*> mSortableTextures;

		std::vector<Program*> mSortablePrograms;

		ImageLoadFunction mImageLoadFunction;

	public:

		explicit ResourceManager(RenderSystem* renderSystem);

		~ResourceManager();

		void setImageLoadFunction(ImageLoadFunction function);

		ImageLoadFunction getImageLoadFunction();

		void createAllResources();

		void destroyAllResources();

		void loadAllResources();

		void unloadAllResources();

		ResourcePtr createResource(std::string const& name, ResourceStreamPtr resourceStream);

		ResourcePtr getResource(std::string const& name, bool nullIfNotFound = false);

		ResourcePtr getDefault2dProgram(mesh::MeshSpecification const& spec, uint32 flags, bool load, std::string descriptor = "");

		ResourcePtr getDefault2dProgram(std::string const& defaultVertexShader, std::string const& defaultFragmentShader, mesh::MeshSpecification const& spec, uint32 flags, bool load, std::string descriptor = "");

		ResourcePtr getDefault3dProgram(mesh::MeshSpecification const& spec, uint32 flags, bool load, std::string descriptor = "");

		ResourcePtr getDefault3dProgram(std::string const& defaultVertexShader, std::string const& defaultFragmentShader, mesh::MeshSpecification const& spec, uint32 flags, bool load, std::string descriptor = "");

		Texture* getTextureBySortId(uint32 id);

		Program* getProgramBySortId(uint32 id);

		std::set<std::string> getProgramAttributes(mesh::MeshSpecification const& spec, uint32_t flags) const;
	};

}
