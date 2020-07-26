#pragma once

#include <vector>

#include "mpp/Batch.h"

namespace mpp
{
	class _MPPAPI QuadBatch : public Batch
	{
		bool mRotate;

	public:

		enum class VertexOptions
		{
			Auto,
			PreferTrianglesToPoints
		};

		enum class TexCoordsOptions
		{
			None,
			TexCoords2,
			TexCoords4
		};
		
		enum class RotationOptions
		{
			None,
			TexCoords
		};

	private:

		VertexOptions mVertexOptions;

		mpp::mesh::Vertex::DataType mPositionType;

		mpp::mesh::Vertex::DataType mTexcoordType;

		TexCoordsOptions mTexCoordOptions;

		RotationOptions mRotationOptions;

		bool mSameSize;

		int mMaxDimX, mMaxDimY;

		ResourcePtr mProgram, mTexture;

		bool mUsePointSprites;

		bool mTextureAtlas;

		int mTexCoordBufferStride;

		int mIndexWidth;

		float mPointSize;

	private:

		void createImpl();

		void writeVertex(float x, float y, float u, float v, bool rotate, int8** mainPtr, int8** texCoordPtr);

		void writeVertex(float x, float y, float u0, float v0, float u1, float v1, bool rotate, int8** mainPtr, int8** texCoordPtr);

		void setSpecificationPointers(VertexBuffer* mainBuffer, VertexBuffer* texCoordBuffer);

		void setMinimumCount(int count);

		void setMainBufferStride();

		void setTexCoordBufferStride();

		int setIndices(uint32* ptr, uint32 base);

		int getVertexCount(int primitiveCount);

		bool useTexCoords() const;

	public:

		QuadBatch(std::string const& name, 
			VertexOptions vertexOptions,
			mpp::mesh::Vertex::DataType positionType,
			mpp::mesh::Vertex::DataType texcoordType,
			ColourOptions colourOptions,
			RotationOptions rotationOptions,
			bool sameSize,
			int maxDimX,
			int maxDimY,
			ResourcePtr program,
			ResourcePtr texture,
			bool textureAtlas,
			int indexWidth,
			uint32 initialCount,
			RenderSystem* renderSystem, 
			ResourceManager* resourceMgr);

		void finishUpdate(int count, bool updateTexCoords);

		bool usePointSprites() const;

		int getPrimitiveCount() const;

		void setPointSize(float size);

		float getPointSize() const;
	};
}
