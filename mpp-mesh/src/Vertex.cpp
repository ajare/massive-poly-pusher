#undef min
#undef max

#include "half/half.hpp"

#include "mpp/mesh/Vertex.h"
#include "mpp/mesh/MppMeshException.h"

using namespace std;

namespace mpp
{
	namespace mesh
	{

		/*
		 * Get the size of a component in 'units'.
		 *
		 */
		int Vertex::getComponentSize(Vertex::Component component)
		{
			switch (component)
			{
			case Vertex::Component::Unused:
				return 0;
			case Vertex::Component::Colour1:
				return 1;
			case Vertex::Component::Position2:
			case Vertex::Component::TexCoord2:
				return 2;
			case Vertex::Component::Position3:
			case Vertex::Component::Normal3:
			case Vertex::Component::TexCoord3:
			case Vertex::Component::Colour3:
				return 3;
			case Vertex::Component::Position4:
			case Vertex::Component::TexCoord4:
			case Vertex::Component::Colour4:
				return 4;
			default:
				throw MppMeshException("Vertex::getComponentSize() unknown component!");
			}
		}

		/*
		 * Get the size of a datatype.
		 *
		 */
		int Vertex::getDataTypeSize(Vertex::DataType dataType)
		{
			switch (dataType)
			{
			case Vertex::DataType::None:
				return 0;
			case Vertex::DataType::Byte:
			case Vertex::DataType::UnsignedByte:
				return sizeof(char);
			case Vertex::DataType::Short:
			case Vertex::DataType::UnsignedShort:
				return sizeof(short);
			case Vertex::DataType::Int:
			case Vertex::DataType::UnsignedInt:
				return sizeof(int);
			case Vertex::DataType::HalfFloat:
				return sizeof(half_float::half);
			case Vertex::DataType::Float:
				return sizeof(float);
			case Vertex::DataType::Double:
				return sizeof(double);
			case Vertex::DataType::Pad1:
				return 1;
			case Vertex::DataType::Pad2:
				return 2;
			case Vertex::DataType::Pad3:
				return 3;
			case Vertex::DataType::Pad4:
				return 4;
			default:
				throw MppMeshException("Vertex::getDataTypeSize() could not get size of data type!");
			}
		}

		string Vertex::getComponentName(Component component)
		{
			switch (component)
			{
			case Component::Position2:
				return "Position2";
			case Component::TexCoord2:
				return "TexCoord2";
			case Component::Position3:
				return "Position3";
			case Component::Normal3:
				return "Normal3";
			case Component::TexCoord3:
				return "TexCoord3";
			case Component::Colour1:
				return "Colour1";
			case Component::Colour3:
				return "Colour3";
			case Component::Position4:
				return "Position4";
			case Component::Colour4:
				return "Colour4";
			default:
				return "Unknown";
			}
		}

		string Vertex::getDataTypeName(DataType dataType)
		{
			switch (dataType)
			{
			case DataType::Byte:
				return "Int8";
			case DataType::UnsignedByte:
				return "UInt8";
			case DataType::Short:
				return "Int16";
			case DataType::UnsignedShort:
				return "UInt16";
			case DataType::Int:
				return "Int32";
			case DataType::UnsignedInt:
				return "UInt32";
			case DataType::HalfFloat:
				return "HalfFloat";
			case DataType::Float:
				return "Float";
			case DataType::Double:
				return "Double";
			case DataType::Pad1:
				return "Pad1";
			case DataType::Pad2:
				return "Pad2";
			case DataType::Pad3:
				return "Pad3";
			case DataType::Pad4:
				return "Pad4";
			default:
				return "Unknown";
			}
		}
	}
}