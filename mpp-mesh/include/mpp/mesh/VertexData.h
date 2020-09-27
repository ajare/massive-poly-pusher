#pragma once

#include <vector>

#undef min
#undef max

#include <half/half.hpp>

#include "Config.h"
#include "Vertex.h"
#include "MeshSpecification.h"

namespace mpp
{
	namespace mesh
	{

		class _MPPMESHAPI VertexData
		{
		public:

			class _MPPMESHAPI Reader
			{
				friend class VertexData;

				VertexData const* mVertexData;

				uint32 mIndex, mOffset;

			private:

				explicit Reader(VertexData const* vertexData)
					: mVertexData(vertexData)
					, mIndex(0)
					, mOffset(0)
				{
				}

			public:

				void rewind()
				{
					mIndex = mOffset = 0;
				}

				void nextVertex()
				{
					uint32 currentVertex = mIndex / mVertexData->mNumComponents;
					mIndex = mVertexData->mNumComponents * (currentVertex + 1);
					mOffset = mVertexData->mStride * (currentVertex + 1);
				}

				template<typename T>
				T unpack()
				{
					switch (mVertexData->mDataTypes[mIndex++])
					{
					case Vertex::DataType::Byte:
						mOffset += sizeof(int8);
						return (T)((int8)mVertexData->mData[mOffset - sizeof(int8)]);

					case Vertex::DataType::UnsignedByte:
						mOffset += sizeof(uint8);
						return (T)((uint8)mVertexData->mData[mOffset - sizeof(uint8)]);

					case Vertex::DataType::Short:
						mOffset += sizeof(int16);
						return (T)(*(int16*)&mVertexData->mData[mOffset - sizeof(int16)]);

					case Vertex::DataType::UnsignedShort:
						mOffset += sizeof(uint16);
						return (T)(*(uint16*)&mVertexData->mData[mOffset - sizeof(uint16)]);

					case Vertex::DataType::Int:
						mOffset += sizeof(int32);
						return (T)(*(int32*)&mVertexData->mData[mOffset - sizeof(int32)]);

					case Vertex::DataType::UnsignedInt:
						mOffset += sizeof(uint32);
						return (T)(*(uint32*)&mVertexData->mData[mOffset - sizeof(uint32)]);

					case Vertex::DataType::HalfFloat:
						mOffset += sizeof(half_float::half);
						return (T)(*(half_float::half*)&mVertexData->mData[mOffset - sizeof(half_float::half)]);

					case Vertex::DataType::Float:
						mOffset += sizeof(float);
						return (T)(*(float*)&mVertexData->mData[mOffset - sizeof(float)]);

					case Vertex::DataType::Double:
						mOffset += sizeof(double);
						return (T)(*(double*)&mVertexData->mData[mOffset - sizeof(double)]);

					default:
						return T();
					}
				}
			};

		private:

			size_t mStride{ 0 };

			size_t mNumComponents{ 0 };

			uint32 mOffset{ 0 };

			std::vector<int8> mData;

			std::vector<Vertex::DataType> mDataTypes;

		public:

			VertexData(MeshSpecification const& spec, size_t numVertices);

			size_t getStride() const;

			size_t getNumVertices() const;

			std::vector<int8> const& getData() const;

			std::vector<int8> getVertexRange(uint32 start, size_t count) const;

			Reader createReader() const;

			VertexData& i8(int8 data);

			VertexData& i8(int8 data1, int8 data2);

			VertexData& i8(int8 data1, int8 data2, int8 data3);

			VertexData& i8(int8 data1, int8 data2, int8 data3, int8 data4);

			VertexData& u8(uint8 data);

			VertexData& u8(uint8 data1, uint8 data2);

			VertexData& u8(uint8 data1, uint8 data2, uint8 data3);

			VertexData& u8(uint8 data1, uint8 data2, uint8 data3, uint8 data4);

			VertexData& i16(int16 data);

			VertexData& i16(int16 data1, int16 data2);

			VertexData& i16(int16 data1, int16 data2, int16 data3);

			VertexData& i16(int16 data1, int16 data2, int16 data3, int16 data4);

			VertexData& u16(uint16 data);

			VertexData& u16(uint16 data1, uint16 data2);

			VertexData& u16(uint16 data1, uint16 data2, uint16 data3);

			VertexData& u16(uint16 data1, uint16 data2, uint16 data3, uint16 data4);

			VertexData& i32(int32 data);

			VertexData& i32(int32 data1, int32 data2);

			VertexData& i32(int32 data1, int32 data2, int32 data3);

			VertexData& i32(int32 data1, int32 data2, int32 data3, int32 data4);

			VertexData& u32(uint32 data);

			VertexData& u32(uint32 data1, uint32 data2);

			VertexData& u32(uint32 data1, uint32 data2, uint32 data3);

			VertexData& u32(uint32 data1, uint32 data2, uint32 data3, uint32 data4);

			VertexData& f16(float data);

			VertexData& f16(float data1, float data2);

			VertexData& f16(float data1, float data2, float data3);

			VertexData& f16(float data1, float data2, float data3, float data4);

			VertexData& f32(float data);

			VertexData& f32(float data1, float data2);

			VertexData& f32(float data1, float data2, float data3);

			VertexData& f32(float data1, float data2, float data3, float data4);

			VertexData& f64(double data);

			VertexData& f64(double data1, double data2);

			VertexData& f64(double data1, double data2, double data3);

			VertexData& f64(double data1, double data2, double data3, double data4);

		};
	}
}