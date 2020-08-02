#pragma once

#include <functional>
#include <vector>

#include "mpp/Batch.h"

namespace mpp
{
	class _MPPAPI TriangleBatch : public Batch
	{
		ResourcePtr mProgram, mTexture;

	protected:

		mpp::mesh::Vertex::DataType mPositionType;

		mpp::mesh::Vertex::DataType mTexcoordType;

		bool mUseTexCoords;

		int mTexCoordBufferStride;

	private:

		void createImpl();

		void writeVertex(float x, float y, float u, float v, int8** mainPtr, int8** texCoordPtr);

		void setMinimumCount(int count);

		void setMainBufferStride();

		void setTexCoordBufferStride();

	protected:

		void setSpecificationPointers(VertexBuffer* mainBuffer, VertexBuffer* texCoordBuffer);

		virtual void postCreate() {}

	public:

		TriangleBatch(std::string const& name,
			mpp::mesh::Vertex::DataType positionType,
			mpp::mesh::Vertex::DataType texcoordType,
			ColourOptions colourOptions,
			bool useDiffuseColour,
			ResourcePtr program,
			ResourcePtr texture,
			uint32 initialCount,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		void finishUpdate(int count, bool updateTexCoords);

		size_t getPositionStride() const;

		size_t getTexCoordStride() const;

		size_t getColourStride() const;

		int getVertexCount(int primitiveCount);
	};
}
