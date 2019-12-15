#pragma once

#include <vector>

#include "mpp/Resource.h"
#include "mpp/Mesh.h"
#include "mpp/Program.h"

namespace mpp
{
	class _MPPAPI Model : public Resource
	{
	protected:

		std::vector<Mesh*> mMeshes;

	private:

		void createImpl();

		void destroyImpl();

		void loadImpl();

		void unloadImpl();

	public:

		Model(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		virtual ~Model();

		int getNumTriangles() const;

		int getNumMeshes() const;

		Mesh const* getMesh(int index) const;

		Mesh* getMesh(int index);

		void setMeshesDynamic();

		void setMeshesStatic();
	};

}