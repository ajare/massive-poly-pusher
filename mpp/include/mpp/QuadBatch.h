#pragma once

#include <vector>

#include "mpp/Batch.h"

namespace mpp
{
	struct QuadBatchOptions
	{
		enum class PrimitiveOptions
		{
			Auto,
			Points,
			Triangles
		};
		
		PrimitiveOptions primitiveOptions{ PrimitiveOptions::Auto };
		mpp::mesh::Vertex::DataType positionType{ mpp::mesh::Vertex::DataType::Float };
		BatchVertexAttribute texcoordAttrib{ mpp::mesh::Vertex::DataType::Float, false };
		BatchVertexAttribute colourAttrib{ mpp::mesh::Vertex::DataType::Float, false };
		bool useDiffuse{ false };
		bool rotate{ false };
		size_t maxSizeX{ 1 }, maxSizeY{ 1 };
		size_t indexWidth{ 32 };
	};

	class _MPPAPI QuadBatch : public Batch
	{
		QuadBatchOptions mOptions;

		bool mSameSize;

		std::string mTexture;

		bool mTextureAtlas;

		float mPointSize;

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
			ResourcePtr texture,
			bool textureAtlas,
			size_t initialCapacity,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		QuadBatch(std::string const& name,
			QuadBatchOptions const& options,
			bool sameSize,
			ResourcePtr texture,
			bool textureAtlas,
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

		bool positionFixed() const;

		bool rotationFixed() const;

		bool texcoordsFixed() const;

		bool colourFixed() const;
	};
}
