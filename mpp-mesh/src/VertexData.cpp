#include "half/half.hpp"

#include "mpp/mesh/VertexData.h"

using namespace std;

namespace mpp
{
	namespace mesh
	{

		VertexData::VertexData(size_t stride)
			: mStride(stride)
		{
			mData.resize(stride);
		}

		size_t VertexData::getStride() const
		{
			return mStride;
		}

		std::vector<uint8> const& VertexData::getData() const
		{
			return mData;
		}

		VertexData& VertexData::i8(int8 data)
		{
			memcpy(&mData[mOffset], &data, sizeof(int8));
			mOffset += sizeof(int8);
			return *this;
		}

		VertexData& VertexData::i8(int8 data1, int8 data2)
		{
			return i8(data1).i8(data2);
		}

		VertexData& VertexData::i8(int8 data1, int8 data2, int8 data3)
		{
			return i8(data1).i8(data2).i8(data3);
		}

		VertexData& VertexData::i8(int8 data1, int8 data2, int8 data3, int8 data4)
		{
			return i8(data1).i8(data2).i8(data3).i8(data4);
		}

		VertexData& VertexData::u8(uint8 data)
		{
			memcpy(&mData[mOffset], &data, sizeof(uint8));
			mOffset += sizeof(uint8);
			return *this;
		}

		VertexData& VertexData::u8(uint8 data1, uint8 data2)
		{
			return u8(data1).u8(data2);
		}

		VertexData& VertexData::u8(uint8 data1, uint8 data2, uint8 data3)
		{
			return u8(data1).u8(data2).u8(data3);
		}

		VertexData& VertexData::u8(uint8 data1, uint8 data2, uint8 data3, uint8 data4)
		{
			return u8(data1).u8(data2).u8(data3).u8(data4);
		}

		VertexData& VertexData::i16(int16 data)
		{
			memcpy(&mData[mOffset], &data, sizeof(int16));
			mOffset += sizeof(int16);
			return *this;
		}

		VertexData& VertexData::i16(int16 data1, int16 data2)
		{
			return i16(data1).i16(data2);
		}

		VertexData& VertexData::i16(int16 data1, int16 data2, int16 data3)
		{
			return i16(data1).i16(data2).i16(data3);
		}

		VertexData& VertexData::i16(int16 data1, int16 data2, int16 data3, int16 data4)
		{
			return i16(data1).i16(data2).i16(data3).i16(data4);
		}

		VertexData& VertexData::u16(uint16 data)
		{
			memcpy(&mData[mOffset], &data, sizeof(uint16));
			mOffset += sizeof(uint16);
			return *this;
		}

		VertexData& VertexData::u16(uint16 data1, uint16 data2)
		{
			return u16(data1).u16(data2);
		}

		VertexData& VertexData::u16(uint16 data1, uint16 data2, uint16 data3)
		{
			return u16(data1).u16(data2).u16(data3);
		}

		VertexData& VertexData::u16(uint16 data1, uint16 data2, uint16 data3, uint16 data4)
		{
			return u16(data1).u16(data2).u16(data3).u16(data4);
		}

		VertexData& VertexData::i32(int32 data)
		{
			memcpy(&mData[mOffset], &data, sizeof(int32));
			mOffset += sizeof(int32);
			return *this;
		}

		VertexData& VertexData::i32(int32 data1, int32 data2)
		{
			return i32(data1).i32(data2);
		}

		VertexData& VertexData::i32(int32 data1, int32 data2, int32 data3)
		{
			return i32(data1).i32(data2).i32(data3);
		}

		VertexData& VertexData::i32(int32 data1, int32 data2, int32 data3, int32 data4)
		{
			return i32(data1).i32(data2).i32(data3).i32(data4);
		}

		VertexData& VertexData::u32(uint32 data)
		{
			memcpy(&mData[mOffset], &data, sizeof(uint32));
			mOffset += sizeof(uint32);
			return *this;
		}

		VertexData& VertexData::u32(uint32 data1, uint32 data2)
		{
			return u32(data1).u32(data2);
		}

		VertexData& VertexData::u32(uint32 data1, uint32 data2, uint32 data3)
		{
			return u32(data1).u32(data2).u32(data3);
		}

		VertexData& VertexData::u32(uint32 data1, uint32 data2, uint32 data3, uint32 data4)
		{
			return u32(data1).u32(data2).u32(data3).u32(data4);
		}

		VertexData& VertexData::f16(float data)
		{
			half_float::half hdata(data);

			memcpy(&mData[mOffset], &hdata, sizeof(half_float::half));
			mOffset += sizeof(half_float::half);
			return *this;
		}

		VertexData& VertexData::f16(float data1, float data2)
		{
			return f16(data1).f16(data2);
		}

		VertexData& VertexData::f16(float data1, float data2, float data3)
		{
			return f16(data1).f16(data2).f16(data3);
		}

		VertexData& VertexData::f16(float data1, float data2, float data3, float data4)
		{
			return f16(data1).f16(data2).f16(data3).f16(data4);
		}

		VertexData& VertexData::f32(float data)
		{
			memcpy(&mData[mOffset], &data, sizeof(float));
			mOffset += sizeof(float);
			return *this;
		}

		VertexData& VertexData::f32(float data1, float data2)
		{
			return f32(data1).f32(data2);
		}

		VertexData& VertexData::f32(float data1, float data2, float data3)
		{
			return f32(data1).f32(data2).f32(data3);
		}

		VertexData& VertexData::f32(float data1, float data2, float data3, float data4)
		{
			return f32(data1).f32(data2).f32(data3).f32(data4);
		}

		VertexData& VertexData::f64(double data)
		{
			memcpy(&mData[mOffset], &data, sizeof(double));
			mOffset += sizeof(double);
			return *this;
		}

		VertexData& VertexData::f64(double data1, double data2)
		{
			return f64(data1).f64(data2);
		}

		VertexData& VertexData::f64(double data1, double data2, double data3)
		{
			return f64(data1).f64(data2).f64(data3);
		}

		VertexData& VertexData::f64(double data1, double data2, double data3, double data4)
		{
			return f64(data1).f64(data2).f64(data3).f64(data4);
		}
	}
}