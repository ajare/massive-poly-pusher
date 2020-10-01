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
			template<typename T> static half_float::half value(T v, bool normalise=false) { return (half_float::half)v; }
			template<typename T> static half_float::half* ptr(T* p) { return (half_float::half*)p; }
			static size_t size() { return sizeof(half_float::half); }
		};

		struct DataTypeFloat
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Float; }
			template<typename T> static float value(T v, bool normalise=false) { return (float)v; }
			template<typename T> static float* ptr(T* p) { return (float*)p; }
			static size_t size() { return sizeof(float); }
		};

		struct DataTypeDouble
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Double; }
			template<typename T> static double value(T v, bool normalise=false) { return (double)v; }
			template<typename T> static double* ptr(T* p) { return (double*)p; }
			static size_t size() { return sizeof(double); }
		};

		struct DataTypeByte
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Byte; }
			
			static int8 value(half_float::half v, bool normalise) { return normalise ? (int8)((v * 0.5f + 0.5f) * 255.0f) - 128 : (int8)v; }
			static int8 value(float v, bool normalise)            { return normalise ? (int8)((v * 0.5f + 0.5f) * 255.0f) - 128 : (int8)v; }
			static int8 value(double v, bool normalise)           { return normalise ? (int8)((v * 0.5 + 0.5) * 255.0) - 128    : (int8)v; }
			static int8 value(int8 v, bool normalise=false)       { return (int8)v; }
			static int8 value(int16 v, bool normalise=false)      { return (int8)v; }
			static int8 value(int32 v, bool normalise=false)      { return (int8)v; }
			static int8 value(uint8 v, bool normalise=false)      { return (int8)v; }
			static int8 value(uint16 v, bool normalise=false)     { return (int8)v; }
			static int8 value(uint32 v, bool normalise=false)     { return (int8)v; }
			
			template<typename T> static int8* ptr(T* p) { return (int8*)p; }
			static size_t size() { return sizeof(int8); }
		};

		struct DataTypeUnsignedByte
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::UnsignedByte; }

			static uint8 value(half_float::half v, bool normalise) { return normalise ? (uint8)(v * 255.0f) : (uint8)v; }
			static uint8 value(float v, bool normalise)            { return normalise ? (uint8)(v * 255.0f) : (uint8)v; }
			static uint8 value(double v, bool normalise)           { return normalise ? (uint8)(v * 255.0)  : (uint8)v; }
			static uint8 value(int8 v, bool normalise=false)       { return (uint8)v; }
			static uint8 value(int16 v, bool normalise=false)      { return (uint8)v; }
			static uint8 value(int32 v, bool normalise=false)      { return (uint8)v; }
			static uint8 value(uint8 v, bool normalise=false)      { return (uint8)v; }
			static uint8 value(uint16 v, bool normalise=false)     { return (uint8)v; }
			static uint8 value(uint32 v, bool normalise=false)     { return (uint8)v; }
			
			template<typename T> static uint8* ptr(T* p) { return (uint8*)p; }
			static size_t size() { return sizeof(uint8); }
		};

		struct DataTypeShort
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Short; }

			static int16 value(half_float::half v, bool normalise) { return normalise ? (int16)((v * 0.5f + 0.5f) * 65535.0f) - 32768 : (int16)v; }
			static int16 value(float v, bool normalise)            { return normalise ? (int16)((v * 0.5f + 0.5f) * 65535.0f) - 32768 : (int16)v; }
			static int16 value(double v, bool normalise)           { return normalise ? (int16)((v * 0.5 + 0.5) * 65535.0) - 32768    : (int16)v; }
			static int16 value(int8 v, bool normalise=false)       { return (int16)v; }
			static int16 value(int16 v, bool normalise=false)      { return (int16)v; }
			static int16 value(int32 v, bool normalise=false)      { return (int16)v; }
			static int16 value(uint8 v, bool normalise=false)      { return (int16)v; }
			static int16 value(uint16 v, bool normalise=false)     { return (int16)v; }
			static int16 value(uint32 v, bool normalise=false)     { return (int16)v; }
		
			template<typename T> static int16* ptr(T* p) { return (int16*)p; }
			static size_t size() { return sizeof(int16); }
		};

		struct DataTypeUnsignedShort
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::UnsignedShort; }

			static uint16 value(half_float::half v, bool normalise) { return normalise ? (uint16)(v * 65535.0f) : (uint16)v; }
			static uint16 value(float v, bool normalise)            { return normalise ? (uint16)(v * 65535.0f) : (uint16)v; }
			static uint16 value(double v, bool normalise)           { return normalise ? (uint16)(v * 65535.0)  : (uint16)v; }
			static uint16 value(int8 v, bool normalise=false)       { return (uint16)v; }
			static uint16 value(int16 v, bool normalise=false)      { return (uint16)v; }
			static uint16 value(int32 v, bool normalise=false)      { return (uint16)v; }
			static uint16 value(uint8 v, bool normalise=false)      { return (uint16)v; }
			static uint16 value(uint16 v, bool normalise=false)     { return (uint16)v; }
			static uint16 value(uint32 v, bool normalise=false)     { return (uint16)v; }
			
			template<typename T> static uint16* ptr(T* p) { return (uint16*)p; }
			static size_t size() { return sizeof(uint16); }
		};

		struct DataTypeInt
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Int; }

			static int32 value(half_float::half v, bool normalise) { return normalise ? (int32)((v * 0.5f + 0.5f) * 4294967295.0f) - 2147483648 : (int32)v; }
			static int32 value(float v, bool normalise)            { return normalise ? (int32)((v * 0.5f + 0.5f) * 4294967295.0f) - 2147483648 : (int32)v; }
			static int32 value(double v, bool normalise)           { return normalise ? (int32)((v * 0.5 + 0.5) * 4294967295.0) - 2147483648    : (int32)v; }
			static int32 value(int8 v, bool normalise=false)       { return (int32)v; }
			static int32 value(int16 v, bool normalise=false)      { return (int32)v; }
			static int32 value(int32 v, bool normalise=false)      { return (int32)v; }
			static int32 value(uint8 v, bool normalise=false)      { return (int32)v; }
			static int32 value(uint16 v, bool normalise=false)     { return (int32)v; }
			static int32 value(uint32 v, bool normalise=false)     { return (int32)v; }
			
			template<typename T> static int32* ptr(T* p) { return (int32*)p; }
			static size_t size() { return sizeof(int32); }
		};

		struct DataTypeUnsignedInt
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::UnsignedInt; }

			static uint32 value(half_float::half v, bool normalise) { return normalise ? (uint32)(v * 4294967295.0f) : (uint32)v; }
			static uint32 value(float v, bool normalise)            { return normalise ? (uint32)(v * 4294967295.0f) : (uint32)v; }
			static uint32 value(double v, bool normalise)           { return normalise ? (uint32)(v * 4294967295.0)  : (uint32)v; }
			static uint32 value(int8 v, bool normalise=false)       { return (uint32)v; }
			static uint32 value(int16 v, bool normalise=false)      { return (uint32)v; }
			static uint32 value(int32 v, bool normalise=false)      { return (uint32)v; }
			static uint32 value(uint8 v, bool normalise=false)      { return (uint32)v; }
			static uint32 value(uint16 v, bool normalise=false)     { return (uint32)v; }
			static uint32 value(uint32 v, bool normalise=false)     { return (uint32)v; }
			
			template<typename T> static uint32* ptr(T* p) { return (uint32*)p; }
			static size_t size() { return sizeof(uint32); }
		};

		struct DataType2_10_10_10
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::Int_2_10_10_10_REV; }
			template<typename T> static int32 value(T v, bool normalise=false) { return (int32)v; }
			template<typename T> static int32* ptr(T* p) { return (int32*)p; }
			static size_t size() { return sizeof(int32); }
		};

		struct DataTypeUnsigned2_10_10_10
		{
			static Vertex::DataType vertexDataType() { return Vertex::DataType::UnsignedInt_2_10_10_10_REV; }
			template<typename T> static uint32 value(T v, bool normalise=false) { return (uint32)v; }
			template<typename T> static uint32* ptr(T* p) { return (uint32*)p; }
			static size_t size() { return sizeof(uint32); }
		};
	}
}