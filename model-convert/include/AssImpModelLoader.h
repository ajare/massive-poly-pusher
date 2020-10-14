#pragma once

#include <vector>
#include <map>
#include <assimp/scene.h>

#include "mpp/mesh/MeshDefinition.h"
#include "mpp/mesh/MeshSpecification.h"

#include <mpp/mesh-specification-parser/ProgramInformation.h>

class AssImpModelLoader
{
	struct VertexDataStreamDefinition
	{
		int8* data;
		mpp::mesh::Vertex::DataType dataType;
		int offset, stride;
	};

	struct MeshDataStreamDefinition
	{
		int triangleCount, vertexCount;
		std::string name;
		std::string material;
		int indexWidth;
		std::vector<uint8> indexData;
		std::map<mpp::mesh::Vertex::Component, VertexDataStreamDefinition> componentStreams;
	};

private:

	typedef std::function<std::string(aiMaterial*, int)> MaterialTransformer;

private:

	std::string mFilename;

	mpp::mesh::MeshSpecification mSpecification;

	uint32 mMaxVerticesPerMesh;

	bool mGenerateColours;

	std::vector<MeshDataStreamDefinition*> mMeshDataDefinitions;

	std::vector<mpp::mesh::MeshDefinition*> mMeshDefinitions;

private:

	void addBuildVertex(aiMesh const* mesh, int index, aiMaterial* material, std::vector<float>& vertices, bool hasPositions, bool hasNormals, bool hasTexCoords, bool hasColours);

	void addBuildFace(uint32 index0, uint32 index1, uint32 index2, std::vector<uint32>& faces);

	void createMeshDataStreams();

	mpp::mesh::MeshSpecification& getMeshSpecification();

	int getNumMeshes() const;

	void getMeshCounts(int meshIndex, int* primitiveCount, int* vertexCount);

	VertexDataStreamDefinition getMeshDataStream(int meshIndex, mpp::mesh::Vertex::Component component) const;

	int getMeshIndexWidth(int meshIndex) const;

	std::vector<uint8> const& getMeshIndexData(int meshIndex) const;

	std::string const& getMeshName(int meshIndex) const;

	std::string const& getMeshMaterial(int meshIndex) const;

	mpp::mesh::MeshDefinition* createMeshDefinition(int triangleCount, std::string const& name, std::string const& material, int indexWidth);

	bool streamsAreTightlyPacked(mpp::mesh::VertexBufferAttributeLayout const& bufferSpec, std::map<mpp::mesh::Vertex::Component, VertexDataStreamDefinition> const& componentStreams);

	int8* copyVertexBufferData(mpp::mesh::VertexBufferAttributeLayout const& bufferSpec, VertexDataStreamDefinition componentStream, int vertexCount, int vertexStride);

	int8* deinterlaceVertexBufferData(mpp::mesh::VertexBufferAttributeLayout const& bufferSpec, std::map<mpp::mesh::Vertex::Component, VertexDataStreamDefinition> const& componentStreams, int vertexCount, int vertexStride);

public:

	AssImpModelLoader(std::string const& filename, mpp::mesh::MeshSpecification const& meshSpec, uint32 maxVerticesPerMesh, bool generateColours);

	~AssImpModelLoader();

	void load();

	int getNumMeshDefinitions() const;

	mpp::mesh::MeshDefinition* getMeshDefinition(int index);
};