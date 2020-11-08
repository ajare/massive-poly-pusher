#pragma once

#include <initializer_list>

#include "mpp/ModelStream.h"

namespace mpp
{
	class _MPPAPI PrimitiveModelStream : public ModelStream
	{
	protected:

		struct MeshDataStreamDefinition
		{
			mesh::MeshSpecification specification;
			std::string name;
			std::string material;

			std::vector<int8_t> vertexData;
			
			float pointSize;
			
			int indexWidth;
			std::vector<uint32_t> indexData;

			// Component data
			int vertexCount, primitiveCount;
			std::map<mpp::mesh::Vertex::Component, VertexDataStreamDefinition> componentStreams;
		};

	private:

		size_t mStrideInBytes;

	protected:

		MeshDataStreamDefinition mMeshDataDefinition;

	protected:

		void createMeshDataStreams();

		std::map<std::string, size_t> getComponentOffsets(size_t& strideInBytes);

		int getNumMeshes() const;

		mesh::MeshSpecification const& getMeshSpecification(int meshIndex) const;

		void getMeshCounts(int meshIndex, int* primitiveCount, int* vertexCount);

		VertexDataStreamDefinition getMeshDataStream(int meshIndex, mesh::Vertex::Component component) const;

		int getMeshIndexWidth(int meshIndex) const;

		float getMeshPointSize(int meshIndex) const;

		uint8_t const* getMeshIndexData(int meshIndex) const;

		std::string const& getMeshName(int meshIndex) const;

		std::string const& getMeshMaterial(int meshIndex) const;

		template<typename T>
		void setVertexData(int offset, std::initializer_list<T> const& vertex)
		{
			uint8_t* dataPtr = ((uint8_t*)&mMeshDataDefinition.vertexData[0]) + offset;
			for (auto it: vertex)
			{
				T value = it;
				memcpy(dataPtr, &value, sizeof(T));
				dataPtr += sizeof(T);
			}
		}

		void addTriangle(uint32_t v0, uint32_t v1, uint32_t v2);

	public:

		PrimitiveModelStream(ResourceManager* resourceMgr, mesh::MeshSpecification const& meshSpec, std::string const& material);

		void setData(int offset, mesh::Vertex::Component component, mesh::Vertex::DataType dataType, bool normalised, double x, double y = 0.0, double z = 0.0, double w = 1.0);
	};
}
