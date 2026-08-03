#pragma once

#include <map>

#include "mpp/Resource.h"
#include "mpp/Texture.h"
#include "mpp/Program.h"
#include "mpp/UniformCollection.h"
#include "mpp/MaterialSpecification.h"

namespace mpp
{
	class _MPPAPI Material : public Resource
	{
		ResourcePtr mProgram;

		UniformCollection mUniforms;

		MaterialSpecification::PbrSurface mPbrSurface;

		std::vector<ResourcePtr> mTextures;

	protected:

		void createImpl();

		void destroyImpl();

		void loadImpl();

		void unloadImpl();

	public:

		Material(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		~Material();

		ResourcePtr getProgram();

		bool isPbr() const;

		MaterialSpecification::PbrSurface const& getPbrSurface() const;

		int getNumTextures() const;

		void setTexture(int i, ResourcePtr texture);

		ResourcePtr getTexture(int i) const;
		
		void setUniforms();
	};
}
