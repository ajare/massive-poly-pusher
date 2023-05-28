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

		static uint32_t msSortableTextureId;

		static uint32_t msSortableProgramId;

		std::vector<ResourcePtr> mSortableTextures;

		std::vector<ResourcePtr> mSortablePrograms;

		ImageLoadFunction mImageLoadFunction;

		bool mLogResourceEvents{ false };

		size_t mProgramIdCounter{ 0 };

	public:

		explicit ResourceManager(RenderSystem* renderSystem);

		~ResourceManager() = default;

		void setImageLoadFunction(ImageLoadFunction function);

		ImageLoadFunction getImageLoadFunction();

		void createAllResources();

		void loadAllResources();

		ResourcePtr declareResource(std::string const& name, ResourceStreamPtr resourceStream, bool loadStream = true, uint32_t quality = 0);

		ResourcePtr acquireResource(std::string const& name);

		ResourcePtr getResource(std::string const& name, bool nullIfNotFound = false);

		ResourcePtr getDefault2dProgram(mesh::MeshSpecification const& spec, uint32_t flags, bool load, std::string descriptor = "");

		ResourcePtr getDefault2dProgram(std::string const& defaultVertexShader, std::string const& defaultFragmentShader, mesh::MeshSpecification const& spec, uint32_t flags, bool load, std::string descriptor = "");

		ResourcePtr getDefault3dProgram(mesh::MeshSpecification const& spec, uint32_t flags, bool load, std::string descriptor = "");

		ResourcePtr getDefault3dProgram(std::string const& defaultVertexShader, std::string const& defaultFragmentShader, mesh::MeshSpecification const& spec, uint32_t flags, bool load, std::string descriptor = "");

		ResourcePtr getTextureBySortId(uint32_t id);

		ResourcePtr getProgramBySortId(uint32_t id);

		std::set<std::string> getProgramAttributes(mesh::MeshSpecification const& spec, uint32_t flags) const;

		void dumpResources(std::string const& filepath);

		void debugMessage(std::string const& message);

		void infoMessage(std::string const& message);

		void warnMessage(std::string const& message);

		void errorMessage(std::string const& message);
	};

}
