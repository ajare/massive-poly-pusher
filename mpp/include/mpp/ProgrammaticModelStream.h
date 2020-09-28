#pragma once

#include <initializer_list>
#include <any>

#include "mpp/ModelStream.h"

#include "mpp/mesh/VertexData.h"

namespace mpp
{

	class _MPPAPI ProgrammaticModelStream : public ModelStream
	{
		struct MeshDataStreamDefinition
		{
			mesh::MeshSpecification specification;
			std::string name;
			std::string material;
			std::vector<uint8> vertexData;
			int indexWidth;
			float pointSize;
			std::vector<uint8> indexData;

			// Component data
			int vertexCount, primitiveCount;
			std::map<mpp::mesh::Vertex::Component, VertexDataStreamDefinition> componentStreams;
		};

	private:

		std::vector<MeshDataStreamDefinition> mMeshDataDefinitions;

	protected:

		void createMeshDataStreams();

		int getNumMeshes() const;

		mesh::MeshSpecification const& getMeshSpecification(int meshIndex) const;

		void getMeshCounts(int meshIndex, int* primitiveCount, int* vertexCount);

		VertexDataStreamDefinition getMeshDataStream(int meshIndex, mesh::Vertex::Component component) const;

		int getMeshIndexWidth(int meshIndex) const;

		float getMeshPointSize(int meshIndex) const;

		uint8 const* getMeshIndexData(int meshIndex) const;

		std::string const& getMeshName(int meshIndex) const;

		std::string const& getMeshMaterial(int meshIndex) const;

	public:

		ProgrammaticModelStream();

		int createMesh(std::string const& name, mesh::MeshSpecification const& specification, std::string const& material, int indexWidth, float pointSize = -1.0f);

		int getMeshId(std::string const& name) const;

		void addVertexData(int meshIndex, std::vector<int8> const& vertexData);

		void addVertexData(int meshIndex, mesh::VertexData const& vertexData);

		void addPoint(int meshIndex, uint32 v);

		void addLine(int meshIndex, uint32 v0, uint32 v1);

		void addTriangle(int meshIndex, uint32 v0, uint32 v1, uint32 v2);
	};
}
