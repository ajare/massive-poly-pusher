#pragma once

#include <map>
#include <vector>
#include <fstream>

#include "mpp/mesh/Vertex.h"
#include "mpp/mesh/MeshDefinition.h"
#include "mpp/mesh/MeshSpecification.h"

#include "mpp/Resource.h"
#include "mpp/ResourceStream.h"
#include "mpp/Program.h"

namespace mpp
{
	class RenderSystem;
	class ResourceManager;

	class _MPPAPI ModelStream : public ResourceStream
	{
	public:

		struct VertexDataStreamDefinition
		{
			std::shared_ptr<const int8> data;
			mesh::Vertex::DataType dataType;
			int offset, stride;
		};

	private:

		std::vector<mesh::MeshDefinition*> mMeshDefinitions;

	private:

		mesh::MeshDefinition* createMeshDefinition(std::string const& name, mesh::Primitive::Type type, int primitiveCount, mesh::VertexBufferStorageType storageType, std::string const& material, int indexWidth, float pointSize = -1.0f);

		bool streamsAreTightlyPacked(mesh::VertexBufferAttributeLayout const& bufferSpec, std::map<mesh::Vertex::Component, VertexDataStreamDefinition> const& componentStreams);

		int8* copyVertexBufferData(mesh::VertexBufferAttributeLayout const& bufferSpec, VertexDataStreamDefinition componentStream, int vertexCount, int vertexStride);

		int8* deinterlaceVertexBufferData(mesh::VertexBufferAttributeLayout const& bufferSpec, std::map<mesh::Vertex::Component, VertexDataStreamDefinition> const& componentStreams, int vertexCount, int vertexStride);

	protected:

		void loadImpl();

		int getNumBufferDefinitions() const;

		virtual void createMeshDataStreams() = 0;

		virtual int getNumMeshes() const = 0;

		virtual mesh::MeshSpecification const& getMeshSpecification(int meshIndex) const = 0;

		virtual void getMeshCounts(int meshIndex, int* primitiveCount, int* vertexCount) = 0;

		virtual VertexDataStreamDefinition getMeshDataStream(int meshIndex, mesh::Vertex::Component component) const = 0;

		virtual int getMeshIndexWidth(int meshIndex) const = 0;

		virtual float getMeshPointSize(int meshIndex) const = 0;

		virtual uint8 const* getMeshIndexData(int meshIndex) const = 0;

		virtual std::string const& getMeshName(int meshIndex) const = 0;

		virtual std::string const& getMeshMaterial(int meshIndex) const = 0;

	public:

		explicit ModelStream(ResourceManager* resourceMgr);

		virtual ~ModelStream();

		int getNumMeshDefinitions() const;

		mesh::MeshDefinition* getMeshDefinition(int index);

		virtual std::string markUpMaterialName(std::string const& name, std::string const& material);

	};
}