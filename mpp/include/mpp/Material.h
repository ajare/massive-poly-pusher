#pragma once

#include <map>

#include "mpp/Resource.h"
#include "mpp/Texture.h"
#include "mpp/Program.h"

namespace mpp
{
	class _MPPAPI Material : public Resource
	{
		template<typename T>
		struct Uniform
		{
			T values[4];
			int valueCount;
		};

		ResourcePtr mProgram;

		std::vector<std::pair<int, Uniform<float>>> mFloatUniforms;

		std::vector<ResourcePtr> mTextures;

	private:

		void createImpl();

		void destroyImpl();

		void loadImpl();

		void unloadImpl();

	public:

		Material(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		ResourcePtr getProgram();

		int getNumTextures() const;

		ResourcePtr getTexture(int i) const;
		
		void setUniforms();
	};
}
