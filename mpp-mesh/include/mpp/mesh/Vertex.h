#pragma once

#include <string>

#include "Config.h"

namespace mpp
{
	namespace mesh
	{

		struct _MPPMESHAPI Vertex
		{
			enum class DataType
			{
				None,
				Byte,
				UnsignedByte,
				Short,
				UnsignedShort,
				Int,
				UnsignedInt,
				HalfFloat,
				Float,
				Double,
				Pad1,
				Pad2,
				Pad3,
				Pad4,
				// GL_INT_2_10_10_10_REV
				// GL_UNSIGNED_INT_2_10_10_10_REV
				// GL_UNSIGNED_INT_10F_11F_11F_REV
			};

			enum class Component
			{
				Unused = 0,
				Position2 = 1 << 0,
				Position3 = 1 << 1,
				Position4 = 1 << 2,
				Normal3 = 1 << 3,
				TexCoord2 = 1 << 4,
				TexCoord3 = 1 << 5,
				TexCoord4 = 1 << 6,
				Colour1 = 1 << 7,
				Colour3 = 1 << 8,
				Colour4 = 1 << 9,
			};

			static int getComponentSize(Component component);

			static int getDataTypeSize(DataType dataType);

			static std::string getComponentName(Component component);

			static std::string getDataTypeName(DataType dataType);
		};

	}
}