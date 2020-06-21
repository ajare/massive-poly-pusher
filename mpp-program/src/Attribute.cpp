#include "utils/StringUtils.h"

#include "Attribute.h"
#include "MppProgramException.h"

namespace mpp
{
	namespace program
	{

		using namespace std;

		string Attribute::getGlslType() const
		{
			string type;
			int componentSize = mesh::Vertex::getComponentSize(component);

			// Get dimension
			switch (componentSize)
			{
			case 1:
				type = ""; break;
			default:
				type = "vec" + utils::StringUtils::toString(componentSize);
			}

			// Get type
			switch (componentSize)
			{
			case 1:
				switch (dataType)
				{
				case mpp::mesh::Vertex::DataType::Float:
				case mpp::mesh::Vertex::DataType::HalfFloat:
					type = "float"; 
					break;
				case mpp::mesh::Vertex::DataType::Double:
					type = "double"; 
					break;
				case mpp::mesh::Vertex::DataType::Byte:
				case mpp::mesh::Vertex::DataType::Short:
				case mpp::mesh::Vertex::DataType::Int:
				case mpp::mesh::Vertex::DataType::Int_2_10_10_10_REV:
					type = normalised ? "float" : "int";
					break;
				case mpp::mesh::Vertex::DataType::UnsignedByte:
				case mpp::mesh::Vertex::DataType::UnsignedShort:
				case mpp::mesh::Vertex::DataType::UnsignedInt:
				case mpp::mesh::Vertex::DataType::UnsignedInt_2_10_10_10_REV:
					type = normalised ? "float" : "int";
					break;
				default:
					THROW_MPP_PROGRAM("Unknown data type.", __LINE__, __FILE__, __FUNCTION__);
				}

			default:
				switch (dataType)
				{
				case mpp::mesh::Vertex::DataType::Double:
					type = "d" + type; break;
				case mpp::mesh::Vertex::DataType::Byte:
				case mpp::mesh::Vertex::DataType::Short:
				case mpp::mesh::Vertex::DataType::Int:
				case mpp::mesh::Vertex::DataType::Int_2_10_10_10_REV:
					if (!normalised)
					{
						type = "i" + type;
					}
					break;
				case mpp::mesh::Vertex::DataType::UnsignedByte:
				case mpp::mesh::Vertex::DataType::UnsignedShort:
				case mpp::mesh::Vertex::DataType::UnsignedInt:
				case mpp::mesh::Vertex::DataType::UnsignedInt_2_10_10_10_REV:
					if (!normalised)
					{
						type = "u" + type;
					}
					break;
				}
			}

			return type;
		}

	}
}