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
		
		int mPrimitiveSize;

		int mPrimitiveCount;

		int mIndexWidth, mIndexDataSize;

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

		void allocateIndexData(int numPrimitives);

		void bind(bool use) const;

	public:

		Mesh(RenderSystem* renderSystem, std::string const& name, ResourcePtr material, mesh::Primitive::Type type, size_t primitiveCount, mesh::VertexBufferStorageType storageType, float pointSize = -1.0f);

		Mesh(RenderSystem* renderSystem, std::string const& name, ResourcePtr material, mesh::Primitive::Type type, size_t primitiveCount, int indexWidth, std::vector<uint8_t> const& indices, mesh::VertexBufferStorageType storageType, float pointSize = -1.0f);

		~Mesh();

		std::string const& getName() const;

		void setMaterial(ResourcePtr material);

		ResourcePtr getMaterial() const;

		void setIndexData(std::vector<uint8_t> const& indexData, int indexWidth);

		bool isIndexed() const;

		void setNumPrimitives(size_t count);

		size_t getNumPrimitives() const;

		void setPointSize(float size);

		float getPointSize() const;

		VertexBuffer* createVertexBuffer(int vertexCount, int vertexStride, bool streaming, bool staticData, std::shared_ptr<const int8_t> vertexData);
		
		int getNumVertexBuffers() const;

		VertexBuffer* getVertexBuffer(int index);

		std::vector<VertexBuffer*> const& getVertexBuffers() const;

		std::vector<uint8_t>& getIndexData();

		void mapIndexData(size_t numVertices);

		void mapIndexData(int startVertex, int numVertices);

		void load();

		void unload();

		void render(float pointSize = -1.0f) const;

		void render(uint32_t numPrimitives, float pointSize = -1.0f) const;

		void setStorageType(mesh::VertexBufferStorageType storageType);

		int getVertexSize() const;
	};
}
