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

		uint32_t mPrimitiveRenderType;

		float mPointSize;
		
		uint32_t mPrimitiveSize;

		size_t mPrimitiveCount;

		size_t mIndexWidth, mIndexDataSize;

		mesh::VertexBufferStorageType mStorageType;

		uint32_t mVAO, mIBO;

		RenderSystem* mwRenderSystem;

		ResourcePtr mMaterial;

		std::vector<VertexBuffer*> mVertexBuffers;

		std::vector<uint8_t> mIndexData;

		bool mIsIndexed;

		bool mIsLoaded;

	private:

		// Methods to be used by RenderSystem
		friend class RenderSystem;

		void setPrimitiveData(mesh::Primitive::Type type);

		void allocateIndexData(size_t numPrimitives);

		void bind(bool use) const;

	public:

		Mesh(RenderSystem* renderSystem, std::string const& name, ResourcePtr material, mesh::Primitive::Type type, size_t primitiveCount, mesh::VertexBufferStorageType storageType, float pointSize = -1.0f);

		Mesh(RenderSystem* renderSystem, std::string const& name, ResourcePtr material, mesh::Primitive::Type type, size_t primitiveCount, size_t indexWidth, std::vector<uint8_t> const& indices, mesh::VertexBufferStorageType storageType, float pointSize = -1.0f);

		~Mesh();

		int getIdCount() const;

		int getLiveIdCount() const;

		std::string const& getName() const;

		void setMaterial(ResourcePtr material);

		ResourcePtr getMaterial() const;

		void setIndexData(std::vector<uint8_t> const& indexData, size_t indexWidth);

		bool isIndexed() const;

		void setNumPrimitives(size_t count);

		size_t getNumPrimitives() const;

		void setPointSize(float size);

		float getPointSize() const;

		VertexBuffer* createVertexBuffer(size_t vertexCount, size_t vertexStride, bool streaming, bool staticData, std::shared_ptr<const int8_t> vertexData);
		
		size_t getNumVertexBuffers() const;

		VertexBuffer* getVertexBuffer(int index);

		std::vector<VertexBuffer*> const& getVertexBuffers() const;

		std::vector<uint8_t>& getIndexData();

		void mapIndexData(size_t numVertices);

		void mapIndexData(int startVertex, int numVertices);

		void load();

		void unload();

		void render(size_t instanceCount) const;

		void render(size_t instanceCount, uint32_t start, size_t count) const;

		void setStorageType(mesh::VertexBufferStorageType storageType);

		size_t getVertexSize() const;
	};
}
