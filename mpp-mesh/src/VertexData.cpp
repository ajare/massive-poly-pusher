#include "half/half.hpp"

#include "mpp/mesh/VertexData.h"

using namespace std;

namespace mpp
{
	namespace mesh
	{

		VertexData::VertexData(MeshSpecification const& spec, size_t numVertices)
			: mOffset(0)
			, mSpec(spec)
		{
			mData.resize(getStride() * numVertices);
		}

		void VertexData::clear()
		{
			mData.clear();
			mDataTypes.clear();
			mOffset = 0;
		}

		MeshSpecification const& VertexData::getMeshSpecification() const
		{
			return mSpec;
		}

		size_t VertexData::getStride() const
		{
			return mSpec.getVertexStrideInBytes();
		}

		size_t VertexData::getNumComponents() const
		{
			return mSpec.getNumComponents();
		}

		size_t VertexData::getNumVertices() const
		{
			return mData.size() / getStride();
		}

		vector<int8_t> const& VertexData::getData() const
		{
			return mData;
		}

		vector<int8_t> VertexData::getVertexRange(uint32_t start, size_t count) const
		{
			return vector<int8_t>(mData.begin() + getStride() * start, mData.begin() + getStride() * (start + count));
		}

		VertexData::Reader VertexData::createReader() const
		{
			return Reader(this);
		}

		VertexData& VertexData::i8(int8_t data)
		{
			memcpy(&mData[mOffset], &data, sizeof(int8_t));
			mOffset += sizeof(int8_t);
			mDataTypes.push_back(Vertex::DataType::Byte);
			return *this;
		}

		VertexData& VertexData::i8(int8_t data1, int8_t data2)
		{
			return i8(data1).i8(data2);
		}

		VertexData& VertexData::i8(int8_t data1, int8_t data2, int8_t data3)
		{
			return i8(data1).i8(data2).i8(data3);
		}

		VertexData& VertexData::i8(int8_t data1, int8_t data2, int8_t data3, int8_t data4)
		{
			return i8(data1).i8(data2).i8(data3).i8(data4);
		}

		VertexData& VertexData::u8(uint8_t data)
		{
			memcpy(&mData[mOffset], &data, sizeof(uint8_t));
			mOffset += sizeof(uint8_t);
			mDataTypes.push_back(Vertex::DataType::UnsignedByte);
			return *this;
		}

		VertexData& VertexData::u8(uint8_t data1, uint8_t data2)
		{
			return u8(data1).u8(data2);
		}

		VertexData& VertexData::u8(uint8_t data1, uint8_t data2, uint8_t data3)
		{
			return u8(data1).u8(data2).u8(data3);
		}

		VertexData& VertexData::u8(uint8_t data1, uint8_t data2, uint8_t data3, uint8_t data4)
		{
			return u8(data1).u8(data2).u8(data3).u8(data4);
		}

		VertexData& VertexData::i16(int16_t data)
		{
			memcpy(&mData[mOffset], &data, sizeof(int16_t));
			mOffset += sizeof(int16_t);
			mDataTypes.push_back(Vertex::DataType::Short);
			return *this;
		}

		VertexData& VertexData::i16(int16_t data1, int16_t data2)
		{
			return i16(data1).i16(data2);
		}

		VertexData& VertexData::i16(int16_t data1, int16_t data2, int16_t data3)
		{
			return i16(data1).i16(data2).i16(data3);
		}

		VertexData& VertexData::i16(int16_t data1, int16_t data2, int16_t data3, int16_t data4)
		{
			return i16(data1).i16(data2).i16(data3).i16(data4);
		}

		VertexData& VertexData::u16(uint16_t data)
		{
			memcpy(&mData[mOffset], &data, sizeof(uint16_t));
			mOffset += sizeof(uint16_t);
			mDataTypes.push_back(Vertex::DataType::UnsignedShort);
			return *this;
		}

		VertexData& VertexData::u16(uint16_t data1, uint16_t data2)
		{
			return u16(data1).u16(data2);
		}

		VertexData& VertexData::u16(uint16_t data1, uint16_t data2, uint16_t data3)
		{
			return u16(data1).u16(data2).u16(data3);
		}

		VertexData& VertexData::u16(uint16_t data1, uint16_t data2, uint16_t data3, uint16_t data4)
		{
			return u16(data1).u16(data2).u16(data3).u16(data4);
		}

		VertexData& VertexData::i32(int32_t data)
		{
			memcpy(&mData[mOffset], &data, sizeof(int32_t));
			mOffset += sizeof(int32_t);
			mDataTypes.push_back(Vertex::DataType::Int);
			return *this;
		}

		VertexData& VertexData::i32(int32_t data1, int32_t data2)
		{
			return i32(data1).i32(data2);
		}

		VertexData& VertexData::i32(int32_t data1, int32_t data2, int32_t data3)
		{
			return i32(data1).i32(data2).i32(data3);
		}

		VertexData& VertexData::i32(int32_t data1, int32_t data2, int32_t data3, int32_t data4)
		{
			return i32(data1).i32(data2).i32(data3).i32(data4);
		}

		VertexData& VertexData::u32(uint32_t data)
		{
			memcpy(&mData[mOffset], &data, sizeof(uint32_t));
			mOffset += sizeof(uint32_t);
			mDataTypes.push_back(Vertex::DataType::UnsignedInt);
			return *this;
		}

		VertexData& VertexData::u32(uint32_t data1, uint32_t data2)
		{
			return u32(data1).u32(data2);
		}

		VertexData& VertexData::u32(uint32_t data1, uint32_t data2, uint32_t data3)
		{
			return u32(data1).u32(data2).u32(data3);
		}

		VertexData& VertexData::u32(uint32_t data1, uint32_t data2, uint32_t data3, uint32_t data4)
		{
			return u32(data1).u32(data2).u32(data3).u32(data4);
		}

		VertexData& VertexData::f16(float data)
		{
			half_float::half hdata(data);

			memcpy(&mData[mOffset], &hdata, sizeof(half_float::half));
			mOffset += sizeof(half_float::half);
			mDataTypes.push_back(Vertex::DataType::HalfFloat);
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
			mDataTypes.push_back(Vertex::DataType::Float);
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
			mDataTypes.push_back(Vertex::DataType::Double);
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

		void VertexData::clipAgainstBoundingBox(float x0, float y0, float x1, float y1)
		{

		}
	}
}