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
		struct DataTypeHalfFloat
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::HalfFloat; }
			template<typename T> static half_float::half value(T v) { return (half_float::half)v; }
			template<typename T> static half_float::half* ptr(T* p) { return (half_float::half*)p; }
			static size_t size() { return sizeof(half_float::half); }
		};

		struct DataTypeFloat
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Float; }
			template<typename T> static float value(T v) { return (float)v; }
			template<typename T> static float* ptr(T* p) { return (float*)p; }
			static size_t size() { return sizeof(float); }
		};

		struct DataTypeDouble
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Double; }
			template<typename T> static double value(T v) { return (double)v; }
			template<typename T> static double* ptr(T* p) { return (double*)p; }
			static size_t size() { return sizeof(double); }
		};

		struct DataTypeByte
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Byte; }
			template<typename T> static int8 value(T v) { return (int8)v; }
			template<typename T> static int8* ptr(T* p) { return (int8*)p; }
			static size_t size() { return sizeof(int8); }
		};

		struct DataTypeUnsignedByte
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::UnsignedByte; }
			template<typename T> static uint8 value(T v) { return (uint8)v; }
			template<typename T> static uint8* ptr(T* p) { return (uint8*)p; }
			static size_t size() { return sizeof(uint8); }
		};

		struct DataTypeShort
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Short; }
			template<typename T> static int16 value(T v) { return (int16)v; }
			template<typename T> static int16* ptr(T* p) { return (int16*)p; }
			static size_t size() { return sizeof(int16); }
		};

		struct DataTypeUnsignedShort
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::UnsignedShort; }
			template<typename T> static uint16 value(T v) { return (uint16)v; }
			template<typename T> static uint16* ptr(T* p) { return (uint16*)p; }
			static size_t size() { return sizeof(uint16); }
		};

		struct DataTypeInt
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Int; }
			template<typename T> static int32 value(T v) { return (int32)v; }
			template<typename T> static int32* ptr(T* p) { return (int32*)p; }
			static size_t size() { return sizeof(int32); }
		};

		struct DataTypeUnsignedInt
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::UnsignedInt; }
			template<typename T> static uint32 value(T v) { return (uint32)v; }
			template<typename T> static uint32* ptr(T* p) { return (uint32*)p; }
			static size_t size() { return sizeof(uint32); }
		};

		struct DataType2_10_10_10
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Int_2_10_10_10_REV; }
			template<typename T> static int32 value(T v) { return (int32)v; }
			template<typename T> static int32* ptr(T* p) { return (int32*)p; }
			static size_t size() { return sizeof(int32); }
		};

		struct DataTypeUnsigned2_10_10_10
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::UnsignedInt_2_10_10_10_REV; }
			template<typename T> static uint32 value(T v) { return (uint32)v; }
			template<typename T> static uint32* ptr(T* p) { return (uint32*)p; }
			static size_t size() { return sizeof(uint32); }
		};
	}
}