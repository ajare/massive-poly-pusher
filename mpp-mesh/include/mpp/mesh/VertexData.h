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

				uint32_t mIndex, mOffset;

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
					uint32_t currentVertex = mIndex / mVertexData->mNumComponents;
					mIndex = mVertexData->mNumComponents * (currentVertex + 1);
					mOffset = mVertexData->mStride * (currentVertex + 1);
				}

				template<typename T>
				T unpack()
				{
					switch (mVertexData->mDataTypes[mIndex++])
					{
					case Vertex::DataType::Byte:
						mOffset += sizeof(int8_t);
						return (T)((int8_t)mVertexData->mData[mOffset - sizeof(int8_t)]);

					case Vertex::DataType::UnsignedByte:
						mOffset += sizeof(uint8_t);
						return (T)((uint8_t)mVertexData->mData[mOffset - sizeof(uint8_t)]);

					case Vertex::DataType::Short:
						mOffset += sizeof(int16_t);
						return (T)(*(int16_t*)&mVertexData->mData[mOffset - sizeof(int16_t)]);

					case Vertex::DataType::UnsignedShort:
						mOffset += sizeof(uint16_t);
						return (T)(*(uint16_t*)&mVertexData->mData[mOffset - sizeof(uint16_t)]);

					case Vertex::DataType::Int:
						mOffset += sizeof(int32_t);
						return (T)(*(int32_t*)&mVertexData->mData[mOffset - sizeof(int32_t)]);

					case Vertex::DataType::UnsignedInt:
						mOffset += sizeof(uint32_t);
						return (T)(*(uint32_t*)&mVertexData->mData[mOffset - sizeof(uint32_t)]);

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

			uint32_t mOffset{ 0 };

			MeshSpecification mSpec;

			std::vector<int8_t> mData;

			std::vector<Vertex::DataType> mDataTypes;

		public:

			VertexData(MeshSpecification const& spec, size_t numVertices);

			void clear();

			size_t getStride() const;

			size_t getNumVertices() const;

			std::vector<int8_t> const& getData() const;

			std::vector<int8_t> getVertexRange(uint32_t start, size_t count) const;

			Reader createReader() const;

			VertexData& i8(int8_t data);

			VertexData& i8(int8_t data1, int8_t data2);

			VertexData& i8(int8_t data1, int8_t data2, int8_t data3);

			VertexData& i8(int8_t data1, int8_t data2, int8_t data3, int8_t data4);

			VertexData& u8(uint8_t data);

			VertexData& u8(uint8_t data1, uint8_t data2);

			VertexData& u8(uint8_t data1, uint8_t data2, uint8_t data3);

			VertexData& u8(uint8_t data1, uint8_t data2, uint8_t data3, uint8_t data4);

			VertexData& i16(int16_t data);

			VertexData& i16(int16_t data1, int16_t data2);

			VertexData& i16(int16_t data1, int16_t data2, int16_t data3);

			VertexData& i16(int16_t data1, int16_t data2, int16_t data3, int16_t data4);

			VertexData& u16(uint16_t data);

			VertexData& u16(uint16_t data1, uint16_t data2);

			VertexData& u16(uint16_t data1, uint16_t data2, uint16_t data3);

			VertexData& u16(uint16_t data1, uint16_t data2, uint16_t data3, uint16_t data4);

			VertexData& i32(int32_t data);

			VertexData& i32(int32_t data1, int32_t data2);

			VertexData& i32(int32_t data1, int32_t data2, int32_t data3);

			VertexData& i32(int32_t data1, int32_t data2, int32_t data3, int32_t data4);

			VertexData& u32(uint32_t data);

			VertexData& u32(uint32_t data1, uint32_t data2);

			VertexData& u32(uint32_t data1, uint32_t data2, uint32_t data3);

			VertexData& u32(uint32_t data1, uint32_t data2, uint32_t data3, uint32_t data4);

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