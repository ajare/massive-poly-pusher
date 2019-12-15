#pragma once

#include <string>
#include <memory>

#include "Config.h"
#include "MeshDefinition.h"
#include "MeshSpecification.h"

namespace mpp
{
	namespace mesh
	{

		class _MPPMESHAPI ModelSerializer
		{
			struct Header
			{
				int versionMajor, versionMinor;
				uint32 flags;
			};

			struct VertexStream
			{
				int vertexCount, vertexStride;
				std::shared_ptr<const int8> vertexData;
			};

			struct Mesh
			{
				int indexWidth;
				std::shared_ptr<const uint8> indexData;
				MeshSpecification specification;
				std::string name;
				std::string material;
				Primitive::Type primitiveType;
				int primitiveCount;
				std::vector<VertexStream> vertexStreams;
			};

		private:

			Header mHeader;

			std::vector<Mesh> mMeshes;

		private:

			// Read
			void readHeader(FILE* fp);

			MeshSpecification readMeshSpecification(FILE* fp);

			void readVertexBufferAttributeLayout(FILE* fp, VertexBufferAttributeLayout* layout);

			void readVertexBuffer(FILE* fp, int meshIndex);

			void readMeshDefinition(FILE* fp, int meshIndex);

			// Write
			void writeHeader(FILE* fp);

			void writeVertexBufferAttributeLayout(FILE* fp, VertexBufferAttributeLayout const& layout);

			void writeMeshSpecification(FILE* fp, MeshSpecification const& meshSpec);

			void writeVertexBuffer(FILE* fp, VertexStream const& vertexStream);

			void writeMeshDefinition(FILE* fp, int meshIndex);

		public:

			ModelSerializer();

			void setMeshCount(int count);

			int getMeshCount() const;

			void setMeshSpecification(int meshIndex, MeshSpecification const& specification);

			MeshSpecification const& getMeshSpecification(int meshIndex) const;

			void setName(int meshIndex, std::string const& name);

			std::string const& getName(int meshIndex) const;

			void setMaterial(int meshIndex, std::string const& material);

			std::string const& getMaterial(int meshIndex) const;

			void setIndexWidth(int meshIndex, int width);

			int getIndexWidth(int meshIndex) const;

			void setIndexData(int meshIndex, std::shared_ptr<const uint8> indexData);

			std::shared_ptr<const uint8> getIndexData(int meshIndex) const;

			void setPrimitiveType(int meshIndex, Primitive::Type primitiveType);

			Primitive::Type getPrimitiveType(int meshIndex) const;

			void setPrimitiveCount(int meshIndex, int primitiveCount);

			int getPrimitiveCount(int meshIndex) const;

			void addVertexStream(int meshIndex, int vertexCount, int vertexStride, std::shared_ptr<const int8> vertexData);

			void getVertexStream(int meshIndex, int index, int* vertexCount, int* vertexStride, std::shared_ptr<const int8>* vertexData);

			void save(std::string const& filename);

			void load(std::string const& filename);
		};
	}
}

