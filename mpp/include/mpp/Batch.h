#pragma once

#include <vector>

#include "mpp/Model.h"

#include "mpp/mesh/Vertex.h"
#include "mpp/mesh/MeshSpecification.h"

namespace mpp
{
	class _MPPAPI Batch : public Model
	{
	public:

		enum class ColourOptions
		{
			None,
			FloatAlpha,
			FloatRGB,
			FloatRGBA,
			UByteAlpha,
			UByteRGB,
			UByteRGBA
		};

	protected:

		int mCurCount, mMaxCount;

		ColourOptions mColourOptions;

		mesh::MeshSpecification mSpecification;

		int mMainBufferStride, mColourOffset;

		bool mUseDiffuse;

		char* mPositionData;

		char* mTexCoordData;

		char* mColourData;

	private:

		virtual void setMainBufferStride() = 0;

		virtual void setTexCoordBufferStride() = 0;

		virtual int getVertexCount(int primitiveCount) = 0;

	protected:

		void writeUByte(uint8 value, int8** ptr);

		void writeFloat(float value, int8** ptr);

		virtual void setSpecificationPointers(VertexBuffer* mainBuffer, VertexBuffer* texCoordBuffer) = 0;

		virtual void setMinimumCount(int count) = 0;

		ResourcePtr createMaterial(std::string const& name, std::string const& texture, uint32 programFlags);

		ResourcePtr createMaterial(std::string const& name, ResourcePtr program, std::string const& texture, uint32 programFlags);

	public:

		Batch(std::string const& name,
			ColourOptions colourOptions,
			bool useDiffuseColour,
			uint32 initialCount,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		virtual ~Batch() = default;

		mesh::MeshSpecification const& getSpecification() const;

		char* getPositionData();

		int getPositionDataSize() const;

		char* getTexCoordData();

		int getTexcoordDataSize() const;

		char* getColourData();

		bool usingColour() const;

		void setCount(int count);

		int getCount() const;

		int getMaxCount() const;

		virtual int getPrimitiveCount() const;

		void startUpdate(int minimumCount);

		virtual void finishUpdate(int count, bool updateTexCoords) = 0;
	};
}
#pragma once
