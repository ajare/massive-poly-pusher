#pragma once

#include <memory>

#include "Config.h"
#include "VertexBufferDefinition.h"
#include "Primitive.h"
#include"VertexBufferStorageType.h"

namespace mpp
{
	namespace mesh
	{

		class _MPPMESHAPI MeshDefinition
		{
			std::string mName;

			Primitive::Type mPrimitiveType;

			VertexBufferStorageType mStorageType;

			int mIndexWidth;

			float mPointSize;

			std::string mMaterial;

			std::vector<VertexBufferDefinition*> mVertexBufferDefinitions;

			// Array, so must supply deleter, ie: std::shared_ptr<uint16> ptr(new uint8[vertexCount], [](uint16 *p) { delete[] p; } );
			std::shared_ptr<const uint8> mIndexData;

			int mNumPrimitives;

			bool mIsIndexed;

		public:

			MeshDefinition(std::string const& material, Primitive::Type type, VertexBufferStorageType storageType, int numPrimitives, int indexWidth, float pointSize = -1.0f);

			~MeshDefinition();
			
			void setName(std::string const& name);

			void foo();

			std::string const& getName() const;

			Primitive::Type getPrimitiveType() const;

			VertexBufferStorageType getStorageType() const;

			int getIndexWidth() const;

			float getPointSize() const;

			std::string const& getMaterial() const;

			void setNumPrimitives(int numPrimitives);

			int getNumPrimitives() const;

			bool isIndexed() const;

			void setIndexData(std::shared_ptr<const uint8> indexData);

			std::shared_ptr<const uint8> getIndexData() const;

			VertexBufferDefinition* createVertexBufferDefinition(VertexBufferAttributeLayout const& spec, int vertexCount, int vertexStride, std::shared_ptr<const int8> vertexData);

			int getNumVertexBufferDefinitions() const;

			VertexBufferDefinition* getVertexBufferDefinition(int index);

		};

	}
}