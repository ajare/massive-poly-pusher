#pragma once

#include <map>

#include "mpp/Resource.h"
#include "mpp/Texture.h"
#include "mpp/Program.h"
#include "mpp/UniformCollection.h"

namespace mpp
{
	class _MPPAPI Material : public Resource
	{
		ResourcePtr mProgram;

		UniformCollection mUniforms;

		std::vector<ResourcePtr> mTextures;

	protected:

		void createImpl();

		void destroyImpl();

		void loadImpl();

		void unloadImpl();

	public:

		Material(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		ResourcePtr getProgram();

		int getNumTextures() const;

		void setTexture(int i, ResourcePtr texture);

		ResourcePtr getTexture(int i) const;
		
		void setUniforms();
	};
}
