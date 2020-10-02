#pragma once

#include <vector>

#include "mpp/Batch.h"

namespace mpp
{
	struct QuadBatchOptions
	{
		enum class VertexOptions
		{
			Auto,
			Points,
			Triangles
		};
		
		VertexOptions vertexOptions;
		mpp::mesh::Vertex::DataType positionType;
		BatchVertexAttribute texcoordAttrib;
		BatchVertexAttribute colourAttrib;
		bool useDiffuse;
		bool rotate;
	};

	class _MPPAPI QuadBatch : public Batch
	{
		QuadBatchOptions mOptions;

		bool mSameSize;

		int mMaxDimX, mMaxDimY;

		std::string mTexture;

		bool mTextureAtlas;

		float mPointSize;

	protected:

		size_t mIndexWidth;

	private:

		bool indexedVertices() const;

		void createIndexData(std::vector<uint8>& data, uint32_t start, size_t count);

	protected:

		mesh::Primitive::Type getPrimitiveType() const;

		void createImpl();

		void createMeshSpecification(mesh::Primitive::Type primitiveType);

	public:

		QuadBatch(std::string const& name,
			QuadBatchOptions const& options,
			bool sameSize,
			int maxDimX,
			int maxDimY,
			ResourcePtr texture,
			bool textureAtlas,
			size_t indexWidth,
			size_t initialCapacity,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		QuadBatch(std::string const& name,
			QuadBatchOptions const& options,
			bool sameSize,
			int maxDimX,
			int maxDimY,
			ResourcePtr texture,
			bool textureAtlas,
			size_t indexWidth,
			size_t initialCapacity,
			std::string const& defaultVertexShader,
			std::string const& defaultFragmentShader,
			std::string const& descriptor,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		size_t getPrimitiveCount(size_t objectCount) const;

		size_t getVertexCount(size_t primitiveCount);

		int getMaxDimX() const;

		int getMaxDimY() const;

		bool usingPointSprites() const;

		bool usingTriangles() const;

		bool rotating() const;

		bool usingTexture() const;

		bool usingTextureAtlas() const;
	};
}
