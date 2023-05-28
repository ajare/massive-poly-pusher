#pragma once

#include <vector>

#include "mpp/Resource.h"
#include "mpp/Mesh.h"
#include "mpp/Program.h"

#include "mpp/mesh/MeshDefinition.h"

namespace mpp
{
	class _MPPAPI Model : public Resource
	{
		glm::vec3 mBounds[2];

		std::vector<ResourcePtr> mMaterials;

	protected:

		std::vector<Mesh*> mMeshes;

	private:

		glm::vec3 readPositionFromStream(int8_t const* stream, mesh::VertexBufferAttributeLayout::Attribute const& attrib);

		void calculateBounds(mesh::VertexBufferAttributeLayout::Attribute const& posAttr, mesh::VertexBufferDefinition const* bufferDef);

	protected:

		void createImpl();

		void destroyImpl();

		void loadImpl();

		void unloadImpl();

		bool checkVertexAttributeMapping(ResourcePtr material, mesh::MeshDefinition* meshDef);

	public:

		Model(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		~Model();

		int getIdCount() const override;

		int getLiveIdCount() const override;

		int getNumTriangles() const;

		int getNumMeshes() const;

		Mesh const* getMesh(int index) const;

		Mesh* getMesh(int index);

		void getBounds(glm::vec3& bMin, glm::vec3& bMax);

		void setMeshesDynamic();

		void setMeshesStatic();
	};

}