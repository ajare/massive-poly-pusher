#pragma once

#include <map>
#include <algorithm>
#include <string>
#include <sstream>
#include <vector>
#include <iterator>
#include <cctype>
#include <locale>

#include "mpp/Config.h"
#include "mpp/Resource.h"
#include "mpp/ResourceStream.h"
#include "mpp/RenderSystem.h"
#include "mpp/MeshSortFlags.h"
#include "mpp/MppException.h"

#include "mpp/mesh/MeshSpecification.h"

namespace mpp
{

	class _MPPAPI ResourceManager
	{
		std::map<std::string, ResourcePtr> mResources;

		std::map<uint32, ResourcePtr> mDefaultPrograms;

		RenderSystem* mwRenderSystem;

		static uint32 msSortableTextureId;

		static uint32 msSortableProgramId;

		std::vector<Texture*> mSortableTextures;

		std::vector<Program*> mSortablePrograms;

	private:

		template<typename T>
		ResourcePtr createResourceImpl(std::string const& name, ResourceStreamPtr resourceStream)
		{
			if (mResources.find(name) != mResources.end())
			{
				THROW_MPP(
					utils::StringUtils::format("Resource '{}' already exists.", name),
					__LINE__, __FILE__, __FUNCTION__);
			}

			if (resourceStream)
			{
				resourceStream->load();
			}

			auto resourcePtr = ResourcePtr(new T(name, mwRenderSystem, this, resourceStream));
			mResources[name] = resourcePtr;

			return resourcePtr;
		}

		template<typename Out>
		void split(std::string const &s, char delim, Out result)
		{
			std::stringstream ss(s);
			std::string item;
			while (getline(ss, item, delim))
			{
				*(result++) = item;
			}
		}

		std::vector<std::string> split(std::string const& s, char delim) 
		{
			std::vector<std::string> elems;
			split(s, delim, std::back_inserter(elems));
			return elems;
		}

		void ltrim(std::string& s)
		{
			s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
				return !std::isspace(ch);
			}));
		}

		// trim from end (in place)
		void rtrim(std::string& s) 
		{
			s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
				return !std::isspace(ch);
			}).base(), s.end());
		}

		// trim from both ends (in place)
		void trim(std::string& s) 
		{
			ltrim(s);
			rtrim(s);
		}

		// trim from start (copying)
		std::string ltrim_copy(std::string s) 
		{
			ltrim(s);
			return s;
		}

		// trim from end (copying)
		std::string rtrim_copy(std::string s) 
		{
			rtrim(s);
			return s;
		}

		// trim from both ends (copying)
		std::string trim_copy(std::string s) 
		{
			trim(s);
			return s;
		}

		// Shader templates
		bool evaluateShaderDirective(std::string const& expression, std::set<std::string> const& attribs);

		bool processShaderLine(std::string const& lineFragment, std::set<std::string> const& attribs, bool prev);

		std::string generateShader(std::string const& templ, std::set<std::string> const& attribs);

		std::set<std::string> getProgramAttributes(mesh::MeshSpecification const& spec, uint32_t flags) const;

	public:

		explicit ResourceManager(RenderSystem* renderSystem);

		~ResourceManager();

		void createAllResources();

		void destroyAllResources();

		void loadAllResources();

		void unloadAllResources();

		template<typename T>
		ResourcePtr createResource(std::string const& name, ResourceStreamPtr resourceStream)
		{
			return createResourceImpl<T>(name, resourceStream);
		}

		ResourcePtr getResource(std::string const& name, bool nullIfNotFound = false);

		ResourcePtr getOrCreateDefault2dProgram(mesh::MeshSpecification const& spec, uint32 flags, bool load);

		Texture* getTextureBySortId(uint32 id);

		Program* getProgramBySortId(uint32 id);
	};

	// Texture and Program can be sorted on for render order.
	template<>
	inline ResourcePtr ResourceManager::createResource<Texture>(std::string const& name, ResourceStreamPtr resourceStream)
	{
		uint64 maxBits = std::min<uint64>(MPP_RENDER_SORT_TEXTURE0_BITS_SIZE, MPP_RENDER_SORT_TEXTURE1_BITS_SIZE);
		if (msSortableTextureId == (uint32)(1 << maxBits))
		{
			std::string errMsg = utils::StringUtils::format("Cannot create Texture resource '{}'.  Limit reached!", name);
			THROW_MPP(errMsg, __LINE__, __FILE__, __FUNCTION__);
		}

		auto resourcePtr = createResourceImpl<Texture>(name, resourceStream);

		Texture* t = ((Texture*)resourcePtr.get());

		t->setSortId(msSortableTextureId++);
		mSortableTextures.push_back(t);

		return resourcePtr;
	}

	template<>
	inline ResourcePtr ResourceManager::createResource<Program>(std::string const& name, ResourceStreamPtr resourceStream)
	{
		if (msSortableProgramId == (1 << MPP_RENDER_SORT_PROGRAM_BITS_SIZE))
		{
			std::string errMsg = utils::StringUtils::format("Cannot create Program resource '{}'.  Limit reached!", name);
			THROW_MPP(errMsg, __LINE__, __FILE__, __FUNCTION__);
		}

		auto resourcePtr = createResourceImpl<Program>(name, resourceStream);
		
		Program* p = ((Program*)resourcePtr.get());
		
		p->setSortId(msSortableProgramId++);
		mSortablePrograms.push_back(p);

		return resourcePtr;
	}
}
