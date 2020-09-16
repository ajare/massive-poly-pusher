#pragma once

#include <vector>

#include "mpp/Batch.h"

namespace mpp
{

	class _MPPAPI QuadBatch : public Batch
	{
	public:

		enum class VertexOptions
		{
			Auto,
			Points,
			Triangles
		};

	private:

		bool mRotate;

		bool mSameSize;

		int mMaxDimX, mMaxDimY;

		std::string mTexture;

		bool mTextureAtlas;

		float mPointSize;

	protected:

		VertexOptions mVertexOptions;

		mpp::mesh::Vertex::DataType mPositionType;

		mpp::mesh::Vertex::DataType mTexcoordType;

		mpp::mesh::Vertex::DataType mColourType;

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
			VertexOptions vertexOptions,
			mpp::mesh::Vertex::DataType positionType,
			mpp::mesh::Vertex::DataType texcoordType,
			mpp::mesh::Vertex::DataType colourType,
			bool rotate,
			bool sameSize,
			int maxDimX,
			int maxDimY,
			std::string const& texture,
			bool textureAtlas,
			size_t indexWidth,
			size_t initialCapacity,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		QuadBatch(std::string const& name,
			VertexOptions vertexOptions,
			mpp::mesh::Vertex::DataType positionType,
			mpp::mesh::Vertex::DataType texcoordType,
			mpp::mesh::Vertex::DataType colourType,
			bool rotate,
			bool sameSize,
			int maxDimX,
			int maxDimY,
			std::string const& texture,
			bool textureAtlas,
			size_t indexWidth,
			size_t initialCapacity,
			std::string const& defaultVertexShader,
			std::string const& defaultFragmentShader,
			std::string const& descriptor,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		void finishUpdate(int count, bool updateTexCoords);

		int getPrimitiveCount(int objectCount) const;

		int getVertexCount(int primitiveCount);

		bool usingPointSprites() const;

		bool rotating() const;

		bool usingTexture() const;

		bool usingTextureAtlas() const;
	};
}
