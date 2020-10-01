#pragma once

#undef min
#undef max

#include "half/half.hpp"

#include "Vertex.h"

namespace mpp
{
	namespace mesh
	{
		template<int N>
		struct VertexComponentSize
		{
			int value = N;
		};

		template<typename T>
		struct VertexDataType
		{
			Vertex::DataType value = Vertex::DataType::None;
		};

		template<>
		struct VertexDataType<int8>
		{
			Vertex::DataType value = Vertex::DataType::Byte;
		};

		template<>
		struct VertexDataType<uint8>
		{
			Vertex::DataType value = Vertex::DataType::UnsignedByte;
		};

		template<>
		struct VertexDataType<int16>
		{
			Vertex::DataType value = Vertex::DataType::Short;
		};

		template<>
		struct VertexDataType<uint16>
		{
			Vertex::DataType value = Vertex::DataType::UnsignedShort;
		};

		template<>
		struct VertexDataType<int32>
		{
			Vertex::DataType value = Vertex::DataType::Int;
		};

		template<>
		struct VertexDataType<uint32>
		{
			Vertex::DataType value = Vertex::DataType::UnsignedInt;
		};

		template<>
		struct VertexDataType<half_float::half>
		{
			Vertex::DataType value = Vertex::DataType::HalfFloat;
		};

		template<>
		struct VertexDataType<float>
		{
			Vertex::DataType value = Vertex::DataType::Float;
		};

		template<>
		struct VertexDataType<double>
		{
			Vertex::DataType value = Vertex::DataType::Double;
		};

		// Type definitions
		template<typename T>
		struct DataTypeHalfFloat
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::HalfFloat; }
			static half_float::half value(T v) { return (half_float::half)v; }
			static half_float::half* ptr(T* p) { return (half_float::half*)p; }
			static size_t size() { return sizeof(half_float::half); }
		};

		template<typename T>
		struct DataTypeFloat
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Float; }
			static float value(T v) { return (float)v; }
			static float* ptr(T* p) { return (float*)p; }
			static size_t size() { return sizeof(float); }
		};

		template<typename T>
		struct DataTypeDouble
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Double; }
			static double value(T v) { return (double)v; }
			static double* ptr(T* p) { return (double*)p; }
			static size_t size() { return sizeof(double); }
		};

		template<typename T>
		struct DataTypeByte
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Byte; }
			static int8 value(T v) { return (int8)v; }
			static int8* ptr(T* p) { return (int8*)p; }
			static size_t size() { return sizeof(int8); }
		};

		template<typename T>
		struct DataTypeUnsignedByte
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::UnsignedByte; }
			static uint8 value(T v) { return (uint8)v; }
			static uint8* ptr(T* p) { return (uint8*)p; }
			static size_t size() { return sizeof(uint8); }
		};

		template<typename T>
		struct DataTypeShort
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Short; }
			static int16 value(T v) { return (int16)v; }
			static int16* ptr(T* p) { return (int16*)p; }
			static size_t size() { return sizeof(int16); }
		};

		template<typename T>
		struct DataTypeUnsignedShort
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::UnsignedShort; }
			static uint16 value(T v) { return (uint16)v; }
			static uint16* ptr(T* p) { return (uint16*)p; }
			static size_t size() { return sizeof(uint16); }
		};

		template<typename T>
		struct DataTypeInt
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Int; }
			static int32 value(T v) { return (int32)v; }
			static int32* ptr(T* p) { return (int32*)p; }
			static size_t size() { return sizeof(int32); }
		};

		template<typename T>
		struct DataTypeUnsignedInt
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::UnsignedInt; }
			static uint32 value(T v) { return (uint32)v; }
			static uint32* ptr(T* p) { return (uint32*)p; }
			static size_t size() { return sizeof(uint32); }
		};

		template<typename T>
		struct DataType2_10_10_10
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Int_2_10_10_10_REV; }
			static int32 value(T v) { return (int32)v; }
			static int32* ptr(T* p) { return (int32*)p; }
			static size_t size() { return sizeof(int32); }
		};

		template<typename T>
		struct DataTypeUnsigned2_10_10_10
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::UnsignedInt_2_10_10_10_REV; }
			static uint32 value(T v) { return (uint32)v; }
			static uint32* ptr(T* p) { return (uint32*)p; }
			static size_t size() { return sizeof(uint32); }
		};
	}
}