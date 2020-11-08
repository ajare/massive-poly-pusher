#include <algorithm>
#include <cassert>

#include <half/half.hpp>

#include "mpp/Config.h"
#include "mpp/PrimitiveModelStream.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{
	PrimitiveModelStream::PrimitiveModelStream(ResourceManager* resourceMgr, mesh::MeshSpecification const& meshSpec, string const& material)
		: ModelStream(resourceMgr)
		, mStrideInBytes(0)
	{
		mMeshDataDefinition.specification = meshSpec;

		mMeshDataDefinition.name = "0";
		mMeshDataDefinition.material = material;
		mMeshDataDefinition.indexWidth = 32;
	}

	void PrimitiveModelStream::createMeshDataStreams()
	{
		int vertexOffset = 0;
		mStrideInBytes = mMeshDataDefinition.specification.getVertexStrideInBytes();

		// Set counts
		mMeshDataDefinition.vertexCount = (int)mMeshDataDefinition.vertexData.size() / mStrideInBytes;

		int elementSize = mesh::Primitive::size(mMeshDataDefinition.specification.getPrimitiveType());
		int indexWidthBytes = mMeshDataDefinition.indexWidth / 8;

		mMeshDataDefinition.primitiveCount = mMeshDataDefinition.specification.verticesIndexed() ? ((mMeshDataDefinition.indexData.size() * sizeof(uint32_t)) / (elementSize * indexWidthBytes)) : (mMeshDataDefinition.vertexCount / elementSize);

		// Go through each component in order, and build streams.
		auto dataPtr = new int8_t[mMeshDataDefinition.vertexData.size()];
		memcpy(dataPtr, &(mMeshDataDefinition.vertexData[0]), mMeshDataDefinition.vertexData.size());

		auto sharedDataPtr = std::shared_ptr<const int8_t>((const int8_t*)dataPtr, [](const int8_t *p) { delete[] p; });

		for (int i = 0; i < mMeshDataDefinition.specification.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto layout = mMeshDataDefinition.specification.getVertexBufferAttributeLayout(i);

			for (int j = 0; j < layout.getNumAttributes(); ++j)
			{
				auto attrib = layout.getAttribute(j);

				VertexDataStreamDefinition vertexStreamDef;

				vertexStreamDef.data = sharedDataPtr;
				vertexStreamDef.dataType = attrib.dataType;
				vertexStreamDef.offset = vertexOffset;
				vertexStreamDef.stride = mStrideInBytes;

				vertexOffset += attrib.sizeInBytes();

				switch (attrib.component)
				{
				case mesh::Vertex::Component::Position2:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Position2] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::Position3:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Position2] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Position3] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::Position4:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Position2] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Position3] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Position4] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::TexCoord2:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::TexCoord2] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::TexCoord3:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::TexCoord2] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::TexCoord3] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::TexCoord4:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::TexCoord2] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::TexCoord3] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::TexCoord4] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::Colour1:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Colour1] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::Colour3:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Colour1] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Colour3] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::Colour4:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Colour3] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Colour4] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::Normal3:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Normal3] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::Normal4:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Normal3] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Normal4] = vertexStreamDef;
					break;
				}
			}
		}
	}

	map<string, size_t> PrimitiveModelStream::getComponentOffsets(size_t& strideInBytes)
	{
		auto const& meshSpec = getMeshSpecification(0);
		map<string, size_t> componentOffsets;
		strideInBytes = 0;

		for (int i = 0; i < meshSpec.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto const& layout = meshSpec.getVertexBufferAttributeLayout(i);

			for (int j = 0; j < layout.getNumAttributes(); ++j)
			{
				auto const& attrib = layout.getAttribute(j);

				size_t componentSize = attrib.sizeInBytes();

				// Get offset for this component
				componentOffsets[mesh::Vertex::getComponentName(attrib.component)] = strideInBytes;

				// Calculate total stride
				strideInBytes += componentSize;
			}
		}

		return componentOffsets;
	}

	int PrimitiveModelStream::getNumMeshes() const
	{
		return 1;
	}

	mesh::MeshSpecification const& PrimitiveModelStream::getMeshSpecification(int meshIndex) const
	{
		return mMeshDataDefinition.specification;
	}

	void PrimitiveModelStream::getMeshCounts(int meshIndex, int* primitiveCount, int* vertexCount)
	{
		*primitiveCount = mMeshDataDefinition.primitiveCount;
		*vertexCount = mMeshDataDefinition.vertexCount;
	}

	ModelStream::VertexDataStreamDefinition PrimitiveModelStream::getMeshDataStream(int meshIndex, mesh::Vertex::Component component) const
	{
		return mMeshDataDefinition.componentStreams.at(component);
	}

	int PrimitiveModelStream::getMeshIndexWidth(int meshIndex) const
	{
		return mMeshDataDefinition.indexWidth;
	}

	float PrimitiveModelStream::getMeshPointSize(int meshIndex) const
	{
		return mMeshDataDefinition.pointSize;
	}

	uint8_t const* PrimitiveModelStream::getMeshIndexData(int meshIndex) const
	{
		return (uint8_t const*)&(mMeshDataDefinition.indexData[0]);
	}

	string const& PrimitiveModelStream::getMeshName(int meshIndex) const
	{
		return mMeshDataDefinition.name;
	}

	string const& PrimitiveModelStream::getMeshMaterial(int meshIndex) const
	{
		return mMeshDataDefinition.material;
	}

	void PrimitiveModelStream::addTriangle(uint32_t v0, uint32_t v1, uint32_t v2)
	{
		mMeshDataDefinition.indexData.push_back(v0);
		mMeshDataDefinition.indexData.push_back(v1);
		mMeshDataDefinition.indexData.push_back(v2);
	}

	void PrimitiveModelStream::setData(int offset, mesh::Vertex::Component component, mesh::Vertex::DataType dataType, bool normalised, double x, double y, double z, double w)
	{
		switch (component)
		{
		case mesh::Vertex::Component::Colour1:
			switch (dataType)
			{
			case mesh::Vertex::DataType::Float:
				setVertexData(offset, { (float)x }); break;
			case mesh::Vertex::DataType::Double:
				setVertexData(offset, { x }); break;
			case mesh::Vertex::DataType::HalfFloat:
				setVertexData(offset, { (half_float::half)((float)x) }); break;
			case mesh::Vertex::DataType::Byte:
				setVertexData(offset, { (int8_t)(normalised ? x * 0xff : x) }); break;
			case mesh::Vertex::DataType::UnsignedByte:
				setVertexData(offset, { (uint8_t)(normalised ? x * 0xff : x) }); break;
			case mesh::Vertex::DataType::Short:
				setVertexData(offset, { (int16_t)(normalised ? x * 0xffff : x) }); break;
			case mesh::Vertex::DataType::UnsignedShort:
				setVertexData(offset, { (uint16_t)(normalised ? x * 0xffff : x) }); break;
			case mesh::Vertex::DataType::Int:
				setVertexData(offset, { (int32_t)(normalised ? x * 0xffffffff : x) }); break;
			case mesh::Vertex::DataType::UnsignedInt:
				setVertexData(offset, { (uint32_t)(normalised ? x * 0xffffffff : x) }); break;
			default:
				THROW_MPP("Unsupported datatype: " + mesh::Vertex::getDataTypeName(dataType), __LINE__, __FILE__, __func__);
			}
			break;
		case mesh::Vertex::Component::Position2:
		case mesh::Vertex::Component::TexCoord2:
			switch (dataType)
			{
			case mesh::Vertex::DataType::Float:
				setVertexData(offset, { (float)x, (float)y }); break;
			case mesh::Vertex::DataType::Double:
				setVertexData(offset, { x, y }); break;
			case mesh::Vertex::DataType::HalfFloat:
				setVertexData(offset, { (half_float::half)((float)x), (half_float::half)((float)y) }); break;
			case mesh::Vertex::DataType::Byte:
				setVertexData(offset, { (int8_t)(normalised ? x * 0xff : x), (int8_t)(normalised ? y * 0xff : y) }); break;
			case mesh::Vertex::DataType::UnsignedByte:
				setVertexData(offset, { (uint8_t)(normalised ? x * 0xff : x), (uint8_t)(normalised ? y * 0xff : y) }); break;
			case mesh::Vertex::DataType::Short:
				setVertexData(offset, { (int16_t)(normalised ? x * 0xffff : x), (int16_t)(normalised ? y * 0xffffffff : y) }); break;
			case mesh::Vertex::DataType::UnsignedShort:
				setVertexData(offset, { (uint16_t)(normalised ? x * 0xffff : x), (uint16_t)(normalised ? y * 0xffffffff : y) }); break;
			case mesh::Vertex::DataType::Int:
				setVertexData(offset, { (int32_t)(normalised ? x * 0xffffffff : x), (int32_t)(normalised ? y * 0xffffffff : y) }); break;
			case mesh::Vertex::DataType::UnsignedInt:
				setVertexData(offset, { (uint32_t)(normalised ? x * 0xffffffff : x), (uint32_t)(normalised ? y * 0xffffffff : y) }); break;
			default:
				THROW_MPP("Unsupported datatype: " + mesh::Vertex::getDataTypeName(dataType), __LINE__, __FILE__, __func__);
			}
			break;
		case mesh::Vertex::Component::Position3:
		case mesh::Vertex::Component::Normal3:
		case mesh::Vertex::Component::TexCoord3:
			switch (dataType)
			{
			case mesh::Vertex::DataType::Float:
				setVertexData(offset, { (float)x, (float)y, (float)z }); break;
			case mesh::Vertex::DataType::Double:
				setVertexData(offset, { x, y, z }); break;
			case mesh::Vertex::DataType::HalfFloat:
				setVertexData(offset, { (half_float::half)((float)x), (half_float::half)((float)y), (half_float::half)((float)z) }); break;
			case mesh::Vertex::DataType::Byte:
				setVertexData(offset, { (int8_t)(normalised ? x * 0xff : x), (int8_t)(normalised ? y * 0xff : y), (int8_t)(normalised ? z * 0xff : z) }); break;
			case mesh::Vertex::DataType::UnsignedByte:
				setVertexData(offset, { (uint8_t)(normalised ? x * 0xff : x), (uint8_t)(normalised ? y * 0xff : y), (uint8_t)(normalised ? z * 0xff : z) }); break;
			case mesh::Vertex::DataType::Short:
				setVertexData(offset, { (int16_t)(normalised ? x * 0xffff : x), (int16_t)(normalised ? y * 0xffff : y), (int16_t)(normalised ? z * 0xffff : z) }); break;
			case mesh::Vertex::DataType::UnsignedShort:
				setVertexData(offset, { (uint16_t)(normalised ? x * 0xffff : x), (uint16_t)(normalised ? y * 0xffff : y), (uint16_t)(normalised ? z * 0xffff : z) }); break;
			case mesh::Vertex::DataType::Int:
				setVertexData(offset, { (int32_t)(normalised ? x * 0xffffffff : x), (int32_t)(normalised ? y * 0xffffffff : y), (int32_t)(normalised ? z * 0xffffffff : z) }); break;
			case mesh::Vertex::DataType::UnsignedInt:
				setVertexData(offset, { (uint32_t)(normalised ? x * 0xffffffff : x), (uint32_t)(normalised ? y * 0xffffffff : y), (uint32_t)(normalised ? z * 0xffffffff : z) }); break;
			default:
				THROW_MPP("Unsupported datatype: " + mesh::Vertex::getDataTypeName(dataType), __LINE__, __FILE__, __func__);
			}
			break;
		case mesh::Vertex::Component::Position4:
		case mesh::Vertex::Component::Normal4:
		case mesh::Vertex::Component::TexCoord4:
		case mesh::Vertex::Component::Colour4:
			switch (dataType)
			{
			case mesh::Vertex::DataType::Float:
				setVertexData(offset, { (float)x, (float)y, (float)z, (float)w }); break;
			case mesh::Vertex::DataType::Double:
				setVertexData(offset, { x, y, z, w }); break;
			case mesh::Vertex::DataType::HalfFloat:
				setVertexData(offset, { (half_float::half)((float)x), (half_float::half)((float)y), (half_float::half)((float)z), (half_float::half)((float)w) }); break;
			case mesh::Vertex::DataType::Byte:
				setVertexData(offset, { (int8_t)(normalised ? x * 0xff : x), (int8_t)(normalised ? y * 0xff : y), (int8_t)(normalised ? z * 0xff : z), (int8_t)(normalised ? w * 0xff : w) }); break;
			case mesh::Vertex::DataType::UnsignedByte:
				setVertexData(offset, { (uint8_t)(normalised ? x * 0xff : x), (uint8_t)(normalised ? y * 0xff : y), (uint8_t)(normalised ? z * 0xff : z), (uint8_t)(normalised ? w * 0xff : w) }); break;
			case mesh::Vertex::DataType::Short:
				setVertexData(offset, { (int16_t)(normalised ? x * 0xffff : x), (int16_t)(normalised ? y * 0xffff : y), (int16_t)(normalised ? z * 0xffff : z), (int16_t)(normalised ? w * 0xffff : w) }); break;
			case mesh::Vertex::DataType::UnsignedShort:
				setVertexData(offset, { (uint16_t)(normalised ? x * 0xffff : x), (uint16_t)(normalised ? y * 0xffff : y), (uint16_t)(normalised ? z * 0xffff : z), (uint16_t)(normalised ? w * 0xffff : w) }); break;
			case mesh::Vertex::DataType::Int:
				setVertexData(offset, { (int32_t)(normalised ? x * 0xffffffff : x), (int32_t)(normalised ? y * 0xffffffff : y), (int32_t)(normalised ? z * 0xffffffff : z), (int32_t)(normalised ? w * 0xffffff : w) }); break;
			case mesh::Vertex::DataType::UnsignedInt:
				setVertexData(offset, { (uint32_t)(normalised ? x * 0xffffffff : x), (uint32_t)(normalised ? y * 0xffffffff : y), (uint32_t)(normalised ? z * 0xffffffff : z), (uint32_t)(normalised ? w * 0xffffffff : w) }); break;
			default:
				THROW_MPP("Unsupported datatype: " + mesh::Vertex::getDataTypeName(dataType), __LINE__, __FILE__, __func__);
			}
			break;
		default:
			THROW_MPP("Invalid component: " + mesh::Vertex::getComponentName(component), __LINE__, __FILE__, __func__);
		}
	}
}