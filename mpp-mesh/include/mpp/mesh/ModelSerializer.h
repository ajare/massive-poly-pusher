#pragma once

#include <string>
#include <memory>
#include <map>
#include <array>

#include "Config.h"
#include "MeshDefinition.h"
#include "MeshSpecification.h"
#include "MaterialInformation.h"

namespace mpp
{
	namespace mesh
	{

		class _MPPMESHAPI ModelSerializer
		{
		public:

			class MetadataReader
			{
				friend class ModelSerializer;

				struct Mesh
				{
					std::string name;
					uint32_t meshSpec;
					uint32_t material;
					Primitive::Type primitiveType;
				};

			private:

				std::map<std::string, uint32_t> mMaterialLookup;
				std::vector<MaterialInformation> mMaterials;
				std::vector<MeshSpecification> mMeshSpecifications;
				std::vector<Mesh> mMeshes;

			private:

				MetadataReader(
					std::map<std::string, uint32_t> const& materialLookup,
					std::vector<MaterialInformation> const& materials,
					std::vector<MeshSpecification> const& meshSpecifications)
					: mMaterialLookup(materialLookup)
					, mMaterials(materials)
					, mMeshSpecifications(meshSpecifications)
				{
				}

				void addMesh(std::string const& name, uint32_t meshSpec, uint32_t material, Primitive::Type type)
				{
					Mesh mesh
					{
						name,
						meshSpec,
						material,
						type
					};

					mMeshes.push_back(mesh);
				}

			public:

				size_t getNumMeshes() const
				{
					return mMeshes.size();
				}

				MaterialInformation const& getMaterialByMeshId(uint32_t id)
				{
					return mMaterials[mMeshes[id].material];
				}

				MeshSpecification const& getMeshSpecificationByMeshId(uint32_t id)
				{
					return mMeshSpecifications[mMeshes[id].meshSpec];
				}

			};

		private:

			struct Header
			{
				int versionMajor, versionMinor;
				uint32_t flags;
			};

			struct Directory
			{
				struct Entry
				{
					enum class Type
					{
						Unused,
						Materials,
						MeshSpecifications,
						VertexData,
						IndexData,
						MeshMetadata,
						COUNT
					};

					Type type{ Type::Unused };
					uint32_t startOffset{ 0 };
					uint32_t endOffset{ 0 };
					size_t count{ 0 };
				};

				std::array<Entry, static_cast<size_t>(Entry::Type::COUNT)> entries;
			};

			struct VertexStream
			{
				size_t vertexCount, vertexStride;
				std::shared_ptr<const int8_t> vertexData;
			};

			struct IndexStream
			{
				size_t indexWidth{ 0 };
				std::shared_ptr<const uint8_t> indexData;
			};

			struct Mesh
			{
				std::string name;
				uint32_t meshSpec;
				uint32_t material;
				Primitive::Type primitiveType;
				int primitiveCount;

				uint32_t indexStream;
				std::vector<uint32_t> vertexStreams;
			};

		private:

			Header mHeader;

			Directory mDirectory;

			// Materials: a material gets added into an array, for writing, and
			// then the material name is mapped to the array index, for meshes
			// to index with.
			std::map<std::string, uint32_t> mMaterialLookup;

			std::vector<MaterialInformation> mMaterials;

			std::vector<MeshSpecification> mMeshSpecifications;

			std::vector<VertexStream> mVertexStreams;

			// Index buffers: we have a lookup which maps index buffer id
			// to mesh id, so we can get the primitive type etc, when writing
			std::map<uint32_t, uint32_t> mIndexStreamLookup;

			std::vector<IndexStream> mIndexStreams;

			std::vector<Mesh> mMeshes;

		private:

			void clear();

			std::string readString(FILE* fp);

			void writeString(FILE* fp, std::string const& str);

			void readHeader(FILE* fp);

			void writeHeader(FILE* fp);

			void readDirectory(FILE* fp);

			Directory::Entry readDirectoryEntry(FILE* fp);

			void updateDirectoryEntry(FILE *fp, Directory::Entry::Type type, uint32_t start, uint32_t end, size_t count);

			void writeDirectory(FILE* fp);

			void writeDirectoryEntry(FILE* fp, Directory::Entry const& entry);

			void readMaterials(FILE* fp);

			MaterialInformation readMaterial(FILE* fp);

			void writeMaterials(FILE* fp);

			void writeMaterial(FILE* fp, MaterialInformation const& matInfo);

			void readMeshSpecifications(FILE* fp);

			MeshSpecification readMeshSpecification(FILE* fp);

			void writeMeshSpecifications(FILE* fp);

			void writeMeshSpecification(FILE* fp, MeshSpecification const& meshSpec);

			void readVertexBuffers(FILE* fp);

			VertexStream readVertexBuffer(FILE* fp);
			
			void readVertexBufferAttributeLayout(FILE* fp, VertexBufferAttributeLayout* layout, uint32_t attribOffset);

			void writeVertexBuffers(FILE* fp);

			void writeVertexBuffer(FILE* fp, VertexStream const& vertexStream);

			void writeVertexBufferAttributeLayout(FILE* fp, VertexBufferAttributeLayout const& layout);

			void readIndexBuffers(FILE* fp);

			IndexStream readIndexBuffer(FILE* fp);

			void writeIndexBuffers(FILE* fp);

			void writeIndexBuffer(FILE* fp, IndexStream const& indexStream, Primitive::Type primitiveType, size_t numPrimitives);

			void readMeshes(FILE* fp);

			Mesh readMesh(FILE* fp);
					
			void writeMeshes(FILE* fp);

			void writeMesh(FILE* fp, Mesh const& mesh, uint32_t index);

		public:

			ModelSerializer();

			void setName(int meshIndex, std::string const& name);

			std::string const& getName(int meshIndex) const;

			void setMeshCount(int count);

			int getMeshCount() const;

			void setPrimitiveType(int meshIndex, Primitive::Type primitiveType);

			Primitive::Type getPrimitiveType(int meshIndex) const;

			void setPrimitiveCount(int meshIndex, int primitiveCount);

			int getPrimitiveCount(int meshIndex) const;

			void setMeshSpecification(int meshIndex, MeshSpecification const& specification);

			MeshSpecification const& getMeshSpecification(int meshIndex) const;

			void addMaterial(std::string const& name, MaterialInformation const& matInfo);

			void setMaterial(int meshIndex, std::string const& material);

			std::string const& getMaterial(int meshIndex) const;

			std::vector<MaterialInformation> const& getMaterials() const;

			void addVertexStream(int meshIndex, int vertexCount, int vertexStride, std::shared_ptr<const int8_t> vertexData);

			void getVertexStream(int meshIndex, int index, int* vertexCount, int* vertexStride, std::shared_ptr<const int8_t>* vertexData);

			void setIndexBuffer(int meshIndex, std::shared_ptr<const uint8_t> indexData, size_t indexWidth);

			std::shared_ptr<const uint8_t> getIndexData(int meshIndex) const;

			int getIndexWidth(int meshIndex) const;

			void save(std::string const& filename);

			void load(std::string const& filename);

			MetadataReader getReader(std::string const& filename = "");
		};
	}
}

