#pragma once

#include "mpp/ModelStream.h"

namespace mpp
{

	class _MPPAPI MppModelStream : public ModelStream
	{
		struct MeshDataStreamDefinition
		{
			mesh::MeshSpecification specification;
			std::string name;
			std::string material;
			mesh::Primitive::Type primitiveType;
			int primitiveCount, vertexCount;
			int indexWidth;
			float pointSize;
			std::shared_ptr<const uint8> indexData;
			std::map<mesh::Vertex::Component, VertexDataStreamDefinition> componentStreams;
		};

	private:

		std::string mFilename;

		std::vector<MeshDataStreamDefinition*> mMeshDataDefinitions;

	private:

		void createMeshDataStreams();

		VertexDataStreamDefinition getMeshDataStream(int meshIndex, mesh::Vertex::Component component) const;

		mesh::MeshSpecification const& getMeshSpecification(int meshIndex) const;

		int getMeshIndexWidth(int meshIndex) const;

		float getMeshPointSize(int meshIndex) const;

		uint8 const* getMeshIndexData(int meshIndex) const;

		std::string const& getMeshName(int meshIndex) const;

		std::string const& getMeshMaterial(int meshIndex) const;

		int getNumMeshes() const;

		void getMeshCounts(int meshIndex, int* primitiveCount, int* vertexCount);

	public:

		explicit MppModelStream(std::string const& filename);

		~MppModelStream();
	};

}
