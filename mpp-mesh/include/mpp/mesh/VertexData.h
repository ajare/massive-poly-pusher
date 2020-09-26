#pragma once

#include <vector>

#include "Config.h"

namespace mpp
{
	namespace mesh
	{

		class _MPPMESHAPI VertexData
		{
			size_t mStride{ 0 };

			uint32 mOffset{ 0 };

			std::vector<uint8> mData;

		public:

			VertexData(size_t stride);

			size_t getStride() const;

			std::vector<uint8> const& getData() const;

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