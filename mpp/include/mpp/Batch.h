#pragma once

#include <vector>

#include "mpp/Model.h"

#include "mpp/mesh/Vertex.h"
#include "mpp/mesh/MeshSpecification.h"

/*
VertexBuffer:
- Stride

VertexAttribute:
- Name (position, size, border colour, etc)
- Attrib (we don't need to use pos/norm/tc/col etc, give them descriptive names)
  - Extended?
- Type
- Size
- Buffer index
- Offset within buffer
- Data pointer
*/

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

	private:

		std::string mDefaultVertexShader, mDefaultFragmentShader;

		std::string mProgramDescriptor;

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
			std::string const& defaultVertexShader,
			std::string const& defaultFragmentShader,
			std::string const& descriptor,
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

	/************************************************************************************/
	/************************************************************************************/
	/************************************************************************************/

	class _MPPAPI Batch2 : public Model
	{
		std::string mDefaultVertexShader, mDefaultFragmentShader;

		std::string mProgramDescriptor;

	protected:

		int mCurCount, mMaxCount;

		mesh::MeshSpecification mSpecification;

		std::map<std::string, std::pair<char*, size_t>> mDataPointers;

	private:

		virtual int getVertexCount(int primitiveCount) = 0;

		virtual void createMeshSpecification(mesh::Primitive::Type primitiveType) = 0;

		virtual void createMesh(Mesh* mesh, size_t vertexCount, size_t bufferSize, std::shared_ptr<const int8> dataPtr) = 0;

	protected:

		void setSpecificationPointers(Mesh* mesh);

		virtual void setMinimumCount(int count) = 0;

		ResourcePtr createMaterial(std::string const& name, std::string const& texture, uint32 programFlags);

		ResourcePtr createMaterial(std::string const& name, ResourcePtr program, std::string const& texture, uint32 programFlags);

	public:

		Batch2(std::string const& name,
			uint32 initialCount,
			std::string const& defaultVertexShader,
			std::string const& defaultFragmentShader,
			std::string const& descriptor,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		virtual ~Batch2() = default;

		mesh::MeshSpecification const& getSpecification() const;

		const std::pair<char*, size_t>& getAttributeData(std::string const& name) const;

		void setCount(int count);

		int getCount() const;

		int getMaxCount() const;

		virtual int getPrimitiveCount() const;

		void startUpdate(int minimumCount);

		virtual void finishUpdate(int count, bool updateTexCoords) = 0;
	};
}
#pragma once
