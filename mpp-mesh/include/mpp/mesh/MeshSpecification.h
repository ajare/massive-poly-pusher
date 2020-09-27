#pragma once

#include <vector>
#include "Config.h"
#include "Vertex.h"
#include "VertexBufferAttributeLayout.h"
#include "Primitive.h"
#include "VertexBufferStorageType.h"

namespace mpp
{
	namespace mesh
	{

		class _MPPMESHAPI MeshSpecification
		{
			Primitive::Type mPrimitiveType;

			VertexBufferStorageType mStorageType;

			bool mIndexedVertices;

			std::vector<VertexBufferAttributeLayout> mVertexBufferAttributeLayouts;

		public:

			MeshSpecification();

			explicit MeshSpecification(Primitive::Type primitiveType);

			MeshSpecification(Primitive::Type primitiveType, VertexBufferStorageType storageType);

			MeshSpecification(MeshSpecification const& other);
			
			void setPrimitiveType(Primitive::Type primitiveType);

			Primitive::Type getPrimitiveType() const;

			void setStorageType(VertexBufferStorageType storageType);

			VertexBufferStorageType getStorageType() const;

			void setIndexedVertices(bool indexed);

			bool verticesIndexed() const;

			VertexBufferAttributeLayout* createVertexBufferAttributeLayout();

			int getNumVertexBufferAttributeLayouts() const;

			VertexBufferAttributeLayout const& getVertexBufferAttributeLayout(int index) const;

			VertexBufferAttributeLayout& getVertexBufferAttributeLayout(int index);

			size_t getVertexStrideInBytes() const;

			size_t getNumComponents() const;

			std::string getDescriptor(std::string const& prefix) const;

			uint32 getHashCode() const;
		};

	}
}