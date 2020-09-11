#pragma once

#include <vector>

#include "mpp/Config.h"
#include "mpp/Material.h"
#include "mpp/VertexBuffer.h"

#include "mpp/mesh/Primitive.h"
#include "mpp/mesh/VertexBufferStorageType.h"

namespace mpp
{
	class RenderSystem;

	class _MPPAPI Mesh
	{
		std::string mName;

		mesh::Primitive::Type mPrimitiveType;

		uint32 mPrimitiveRenderType;

		float mPointSize;
		
		int mPrimitiveSize;

		int mPrimitiveCount;

		int mIndexWidth, mIndexDataSize;

		mesh::VertexBufferStorageType mStorageType;

		uint32 mVAO, mIBO;

		RenderSystem* mwRenderSystem;

		ResourcePtr mMaterial;

		std::vector<VertexBuffer*> mVertexBuffers;

		std::vector<uint8> mIndexData;

		bool mIsIndexed;

		bool mIsLoaded;

	private:

		// Methods to be used by RenderSystem
		friend class RenderSystem;

		void setPrimitiveData(mesh::Primitive::Type type);

		void allocateIndexData(int numPrimitives);

		void bind(bool use) const;

	public:

		Mesh(RenderSystem* renderSystem, std::string const& name, ResourcePtr material, mesh::Primitive::Type type, int primitiveCount, mesh::VertexBufferStorageType storageType, float pointSize = -1.0f);

		Mesh(RenderSystem* renderSystem, std::string const& name, ResourcePtr material, mesh::Primitive::Type type, int primitiveCount, int indexWidth, std::vector<uint8> const& indices, mesh::VertexBufferStorageType storageType, float pointSize = -1.0f);

		~Mesh();

		std::string const& getName() const;

		void setMaterial(ResourcePtr material);

		ResourcePtr getMaterial() const;

		void setIndexData(std::vector<uint8> const& indexData, int indexWidth);

		bool isIndexed() const;

		void setNumPrimitives(int count);

		int getNumPrimitives() const;

		void setPointSize(float size);

		float getPointSize() const;

		VertexBuffer* createVertexBuffer(int vertexCount, int vertexStride, bool streaming, std::shared_ptr<const int8> vertexData);
		
		int getNumVertexBuffers() const;

		VertexBuffer* getVertexBuffer(int index);

		std::vector<VertexBuffer*> const& getVertexBuffers() const;

		std::vector<uint8>& getIndexData();

		void mapIndexData(int numVertices);

		void mapIndexData(int startVertex, int numVertices);

		void load();

		void unload();

		void render() const;

		void render(uint32 numPrimitives) const;

		void setStorageType(mesh::VertexBufferStorageType storageType);

		int getVertexSize() const;
	};
}
