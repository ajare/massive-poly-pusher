#pragma once

#include <string>
#include <memory>
#include <map>
#include <array>
#include <fstream>

#include "mpp/MaterialSpecification.h"

#include "mpp/mesh/MeshDefinition.h"
#include "mpp/mesh/MeshSpecification.h"

#include "Config.h"

namespace mpp
{

	class _MPPAPI ModelSerializer
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
				mpp::mesh::Primitive::Type primitiveType;
			};

		private:

			std::map<std::string, uint32_t> mMaterialLookup;
			std::vector<MaterialSpecification> mMaterials;
			std::vector<mpp::mesh::MeshSpecification> mMeshSpecifications;
			std::vector<Mesh> mMeshes;

		private:

			MetadataReader(
				std::map<std::string, uint32_t> const& materialLookup,
				std::vector<MaterialSpecification> const& materials,
				std::vector<mpp::mesh::MeshSpecification> const& meshSpecifications)
				: mMaterialLookup(materialLookup)
				, mMaterials(materials)
				, mMeshSpecifications(meshSpecifications)
			{
			}

			void addMesh(std::string const& name, uint32_t meshSpec, uint32_t material, mpp::mesh::Primitive::Type type)
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

			MaterialSpecification const& getMaterialByMeshId(uint32_t id)
			{
				return mMaterials[mMeshes[id].material];
			}

			mpp::mesh::MeshSpecification const& getMeshSpecificationByMeshId(uint32_t id)
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
				std::streampos startOffset{ 0 };
				std::streampos endOffset{ 0 };
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
			mpp::mesh::Primitive::Type primitiveType;
			size_t primitiveCount;

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

		std::vector<MaterialSpecification> mMaterials;

		std::vector<mpp::mesh::MeshSpecification> mMeshSpecifications;

		std::vector<VertexStream> mVertexStreams;

		// Index buffers: we have a lookup which maps index buffer id
		// to mesh id, so we can get the primitive type etc, when writing
		std::map<uint32_t, uint32_t> mIndexStreamLookup;

		std::vector<IndexStream> mIndexStreams;

		std::vector<Mesh> mMeshes;

	private:

		void clear();

		// Write
		void writeValue(std::string const& value, std::ofstream& fp);

		void writeValue(char const* value, size_t count, std::ofstream& fp);

		void writeValue(int32_t value, std::ofstream& fp);

		void writeValue(uint32_t value, std::ofstream& fp);

		void writeValue(int16_t value, std::ofstream& fp);

		void writeValue(uint16_t value, std::ofstream& fp);

		void writeValue(float value, std::ofstream& fp);

		void writeValue(bool value, std::ofstream& fp);

		// Read
		std::string readString(std::ifstream& fp);

		std::string readBytes(size_t count, std::ifstream& fp);

		int32_t readInt32(std::ifstream& fp);

		uint32_t readUInt32(std::ifstream& fp);

		int16_t readInt16(std::ifstream& fp);

		uint16_t readUInt16(std::ifstream& fp);

		float readFloat(std::ifstream& fp);

		bool readBool(std::ifstream& fp);

		void readHeader(std::ifstream& fp);

		void writeHeader(std::ofstream& fp);

		void readDirectory(std::ifstream& fp);

		Directory::Entry readDirectoryEntry(std::ifstream& fp);

		void updateDirectoryEntry(std::ofstream& fp, Directory::Entry::Type type, std::streampos start, std::streampos end, size_t count);

		void writeDirectory(std::ofstream& fp);

		void writeDirectoryEntry(std::ofstream& fp, Directory::Entry const& entry);

		void readMaterials(std::ifstream& fp);

		MaterialSpecification readMaterial(std::ifstream& fp);

		void writeMaterials(std::ofstream& fp);

		void writeMaterial(std::ofstream& fp, MaterialSpecification const& matSpec);

		void readMeshSpecifications(std::ifstream& fp);

		mpp::mesh::MeshSpecification readMeshSpecification(std::ifstream& fp);

		void writeUniformCollection(UniformCollection const& uniforms, std::ofstream& fp);

		UniformCollection readUniformCollection(std::ifstream& fp);

		void writeMeshSpecifications(std::ofstream& fp);

		void writeMeshSpecification(mpp::mesh::MeshSpecification const& meshSpec, std::ofstream& fp);

		void readVertexBuffers(std::ifstream& fp);

		VertexStream readVertexBuffer(std::ifstream& fp);
			
		void readVertexBufferAttributeLayout(std::ifstream& fp, mpp::mesh::VertexBufferAttributeLayout* layout, uint32_t attribOffset);

		void writeVertexBuffers(std::ofstream& fp);

		void writeVertexBuffer(std::ofstream& fp, VertexStream const& vertexStream);

		void writeVertexBufferAttributeLayout(FILE* fp, mpp::mesh::VertexBufferAttributeLayout const& layout);

		void readIndexBuffers(std::ifstream& fp);

		IndexStream readIndexBuffer(std::ifstream& fp);

		void writeIndexBuffers(std::ofstream& fp);

		void writeIndexBuffer(std::ofstream& fp, IndexStream const& indexStream, mpp::mesh::Primitive::Type primitiveType, size_t numPrimitives);

		void readMeshes(std::ifstream& fp);

		Mesh readMesh(std::ifstream& fp);
					
		void writeMeshes(std::ofstream& fp);

		void writeMesh(std::ofstream& fp, Mesh const& mesh, uint32_t index);

	public:

		ModelSerializer();

		void setName(size_t meshIndex, std::string const& name);

		std::string const& getName(size_t meshIndex) const;

		void setMeshCount(size_t count);

		size_t getMeshCount() const;

		void setPrimitiveType(size_t meshIndex, mpp::mesh::Primitive::Type primitiveType);

		mpp::mesh::Primitive::Type getPrimitiveType(size_t meshIndex) const;

		void setPrimitiveCount(size_t meshIndex, size_t primitiveCount);

		int getPrimitiveCount(size_t meshIndex) const;

		void setMeshSpecification(size_t meshIndex, mpp::mesh::MeshSpecification const& specification);

		mpp::mesh::MeshSpecification const& getMeshSpecification(size_t meshIndex) const;

		void addMaterial(std::string const& name, mpp::mesh::MaterialInformation const& matInfo);

		void setMaterial(size_t meshIndex, std::string const& material);

		std::string const& getMaterial(size_t meshIndex) const;

		std::vector<mpp::mesh::MaterialInformation> const& getMaterials() const;

		void addVertexStream(size_t meshIndex, size_t vertexCount, size_t vertexStride, std::shared_ptr<const int8_t> vertexData);

		void getVertexStream(size_t meshIndex, size_t index, size_t* vertexCount, size_t* vertexStride, std::shared_ptr<const int8_t>* vertexData);

		void setIndexBuffer(size_t meshIndex, std::shared_ptr<const uint8_t> indexData, size_t indexWidth);

		std::shared_ptr<const uint8_t> getIndexData(size_t meshIndex) const;

		int getIndexWidth(size_t meshIndex) const;

		void save(std::string const& filename);

		void load(std::string const& filename);

		MetadataReader getReader(std::string const& filename = "");
	};
}

