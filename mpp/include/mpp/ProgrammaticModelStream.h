#pragma once

#include <initializer_list>

#include "mpp/ModelStream.h"

namespace mpp
{
	class _MPPAPI ProgrammaticModelStream : public ModelStream
	{
		struct MeshDataStreamDefinition
		{
			mesh::MeshSpecification specification;
			std::string name;
			std::string material;
			std::vector<float> vertexData;
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
		
		template<typename T>
		void addVertexData(int meshIndex, std::initializer_list<T> const& vertex)
		{
			MeshDataStreamDefinition& meshDef = mMeshDataDefinitions[meshIndex];
			meshDef.vertexData.insert(meshDef.vertexData.end(), vertex.begin(), vertex.end());
		}

		void addVertexData(int meshIndex, std::vector<float>::const_iterator begin, std::vector<float>::const_iterator end)
		{
			MeshDataStreamDefinition& meshDef = mMeshDataDefinitions[meshIndex];
			meshDef.vertexData.insert(meshDef.vertexData.end(), begin, end);
		}

		void addPoint(int meshIndex, uint32 v);

		void addLine(int meshIndex, uint32 v0, uint32 v1);

		void addTriangle(int meshIndex, uint32 v0, uint32 v1, uint32 v2);
	};
}
