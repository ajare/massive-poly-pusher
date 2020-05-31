#include <algorithm>
#include <cassert>

#include <half/half.hpp>

#include "mpp/Config.h"
#include "mpp/GridModelStream.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{
	GridModelStream::GridModelStream(mesh::MeshSpecification const& meshSpec, string const& material, double width, double depth, int dimX, int dimZ)
		: PrimitiveModelStream(meshSpec, material)
	{
		int strideInBytes = 0;
		map<string, int> componentOffsets;
		for (int i = 0; i < meshSpec.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto const& layout = meshSpec.getVertexBufferAttributeLayout(i);

			for (int j = 0; j < layout.getNumAttributes(); ++j)
			{
				auto const& attrib = layout.getAttribute(j);

				int componentSize = attrib.sizeInBytes();

				// Get offset for this component
				componentOffsets[mesh::Vertex::getComponentName(attrib.component)] = strideInBytes;

				// Calculate total stride
				strideInBytes += componentSize;
			}
		}

		// Preallocate vertex buffer
		const int numVertices = meshSpec.verticesIndexed() ? (dimX + 1) * (dimZ + 1) : (dimX * dimZ) * 6;
		int bufferSize = strideInBytes * numVertices;

		mMeshDataDefinition.vertexData.resize(bufferSize);

		// Generate vertices
		double w2 = width / 2;
		double d2 = depth / 2;
		double dw = width / dimX;
		double dh = depth / dimZ;

		for (int i = 0; i < meshSpec.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto const& layout = meshSpec.getVertexBufferAttributeLayout(i);

			for (int j = 0; j < layout.getNumAttributes(); ++j)
			{
				auto const& attrib = layout.getAttribute(j);

				// Get offset and stride for component
				int offset = componentOffsets[mesh::Vertex::getComponentName(attrib.component)];

				switch (attrib.component)
				{
				case mesh::Vertex::Component::Position3:
					switch (attrib.dataType)
					{
					case mesh::Vertex::DataType::Double:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<double>(offset, { -w2 + dw * x, 0.0, -d2 + dh * z });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<double>(offset, { -w2 + dw * x, 0.0, -d2 + dh * z });
									offset += strideInBytes;

									setVertexData<double>(offset, { -w2 + dw * x, 0.0, -d2 + dh * (z + 1) });
									offset += strideInBytes;

									setVertexData<double>(offset, { -w2 + dw * (x + 1), 0.0, -d2 + dh * (z + 1) });
									offset += strideInBytes;

									setVertexData<double>(offset, { -w2 + dw * (x + 1), 0.0, -d2 + dh * (z + 1) });
									offset += strideInBytes;

									setVertexData<double>(offset, { -w2 + dw * (x + 1), 0.0, -d2 + dh * z });
									offset += strideInBytes;

									setVertexData<double>(offset, { -w2 + dw * x, 0.0, -d2 + dh * z });
									offset += strideInBytes;
								}
							}
						}
						break;

					case mesh::Vertex::DataType::Float:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<float>(offset, { (float)(-w2 + dw * x), 0.0f, (float)(-d2 + dh * z) });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<float>(offset, { (float)(-w2 + dw * x), 0.0f, (float)(-d2 + dh * z) });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(-w2 + dw * x), 0.0f, (float)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(-w2 + dw * (x + 1)), 0.0f, (float)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(-w2 + dw * (x + 1)), 0.0f, (float)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(-w2 + dw * (x + 1)), 0.0f, (float)(-d2 + dh * z) });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(-w2 + dw * x), 0.0f, (float)(-d2 + dh * z) });
									offset += strideInBytes;
								}
							}
						}
						break;

					case mesh::Vertex::DataType::HalfFloat:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<half_float::half>(offset, { (half_float::half )(-w2 + dw * x), (half_float::half)0, (half_float::half)(-d2 + dh * z) });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<half_float::half>(offset, { (half_float::half)(-w2 + dw * x), (half_float::half)0, (half_float::half)(-d2 + dh * z) });
									offset += strideInBytes;

									setVertexData<half_float::half>(offset, { (half_float::half)(-w2 + dw * x), (half_float::half)0, (half_float::half)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<half_float::half>(offset, { (half_float::half)(-w2 + dw * (x + 1)), (half_float::half)0, (half_float::half)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<half_float::half>(offset, { (half_float::half)(-w2 + dw * (x + 1)), (half_float::half)0, (half_float::half)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<half_float::half>(offset, { (half_float::half)(-w2 + dw * (x + 1)), (half_float::half)0, (half_float::half)(-d2 + dh * z) });
									offset += strideInBytes;

									setVertexData<half_float::half>(offset, { (half_float::half)(-w2 + dw * x), (half_float::half)0, (half_float::half)(-d2 + dh * z) });
									offset += strideInBytes;
								}
							}
						}
						break;

					case mesh::Vertex::DataType::Byte:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<int8_t>(offset, { (int8_t)(-w2 + dw * x), 0, (int8_t)(-d2 + dh * z) });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<int8_t>(offset, { (int8_t)(-w2 + dw * x), 0, (int8_t)(-d2 + dh * z) });
									offset += strideInBytes;

									setVertexData<int8_t>(offset, { (int8_t)(-w2 + dw * x), 0, (int8_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<int8_t>(offset, { (int8_t)(-w2 + dw * (x + 1)), 0, (int8_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<int8_t>(offset, { (int8_t)(-w2 + dw * (x + 1)), 0, (int8_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<int8_t>(offset, { (int8_t)(-w2 + dw * (x + 1)), 0, (int8_t)(-d2 + dh * z) });
									offset += strideInBytes;

									setVertexData<int8_t>(offset, { (int8_t)(-w2 + dw * x), 0, (int8_t)(-d2 + dh * z) });
									offset += strideInBytes;
								}
							}
						}
						break;

					case mesh::Vertex::DataType::UnsignedByte:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint8_t>(offset, { (uint8_t)(-w2 + dw * x), 0, (uint8_t)(-d2 + dh * z) });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<uint8_t>(offset, { (uint8_t)(-w2 + dw * x), 0, (uint8_t)(-d2 + dh * z) });
									offset += strideInBytes;

									setVertexData<uint8_t>(offset, { (uint8_t)(-w2 + dw * x), 0, (uint8_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<uint8_t>(offset, { (uint8_t)(-w2 + dw * (x + 1)), 0, (uint8_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<uint8_t>(offset, { (uint8_t)(-w2 + dw * (x + 1)), 0, (uint8_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<uint8_t>(offset, { (uint8_t)(-w2 + dw * (x + 1)), 0, (uint8_t)(-d2 + dh * z) });
									offset += strideInBytes;

									setVertexData<uint8_t>(offset, { (uint8_t)(-w2 + dw * x), 0, (uint8_t)(-d2 + dh * z) });
									offset += strideInBytes;
								}
							}
						}
						break;

					case mesh::Vertex::DataType::Short:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<int16_t>(offset, { (int16_t)(-w2 + dw * x), 0, (int16_t)(-d2 + dh * z) });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<int16_t>(offset, { (int16_t)(-w2 + dw * x), 0, (int16_t)(-d2 + dh * z) });
									offset += strideInBytes;

									setVertexData<int16_t>(offset, { (int16_t)(-w2 + dw * x), 0, (int16_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<int16_t>(offset, { (int16_t)(-w2 + dw * (x + 1)), 0, (int16_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<int16_t>(offset, { (int16_t)(-w2 + dw * (x + 1)), 0, (int16_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<int16_t>(offset, { (int16_t)(-w2 + dw * (x + 1)), 0, (int16_t)(-d2 + dh * z) });
									offset += strideInBytes;

									setVertexData<int16_t>(offset, { (int16_t)(-w2 + dw * x), 0, (int16_t)(-d2 + dh * z) });
									offset += strideInBytes;
								}
							}
						}
						break;

					case mesh::Vertex::DataType::UnsignedShort:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint16_t>(offset, { (uint16_t)(-w2 + dw * x), 0, (uint16_t)(-d2 + dh * z) });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<uint16_t>(offset, { (uint16_t)(-w2 + dw * x), 0, (uint16_t)(-d2 + dh * z) });
									offset += strideInBytes;

									setVertexData<uint16_t>(offset, { (uint16_t)(-w2 + dw * x), 0, (uint16_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<uint16_t>(offset, { (uint16_t)(-w2 + dw * (x + 1)), 0, (uint16_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<uint16_t>(offset, { (uint16_t)(-w2 + dw * (x + 1)), 0, (uint16_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<uint16_t>(offset, { (uint16_t)(-w2 + dw * (x + 1)), 0, (uint16_t)(-d2 + dh * z) });
									offset += strideInBytes;

									setVertexData<uint16_t>(offset, { (uint16_t)(-w2 + dw * x), 0, (uint16_t)(-d2 + dh * z) });
									offset += strideInBytes;
								}
							}
						}
						break;

					case mesh::Vertex::DataType::Int:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<int32_t>(offset, { (int32_t)(-w2 + dw * x), 0, (int32_t)(-d2 + dh * z) });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<int32_t>(offset, { (int32_t)(-w2 + dw * x), 0, (int32_t)(-d2 + dh * z) });
									offset += strideInBytes;

									setVertexData<int32_t>(offset, { (int32_t)(-w2 + dw * x), 0, (int32_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<int32_t>(offset, { (int32_t)(-w2 + dw * (x + 1)), 0, (int32_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<int32_t>(offset, { (int32_t)(-w2 + dw * (x + 1)), 0, (int32_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<int32_t>(offset, { (int32_t)(-w2 + dw * (x + 1)), 0, (int32_t)(-d2 + dh * z) });
									offset += strideInBytes;

									setVertexData<int32_t>(offset, { (int32_t)(-w2 + dw * x), 0, (int32_t)(-d2 + dh * z) });
									offset += strideInBytes;
								}
							}
						}
						break;

					case mesh::Vertex::DataType::UnsignedInt:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint32_t>(offset, { (uint32_t)(-w2 + dw * x), 0, (uint32_t)(-d2 + dh * z) });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<uint32_t>(offset, { (uint32_t)(-w2 + dw * x), 0, (uint32_t)(-d2 + dh * z) });
									offset += strideInBytes;

									setVertexData<uint32_t>(offset, { (uint32_t)(-w2 + dw * x), 0, (uint32_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<uint32_t>(offset, { (uint32_t)(-w2 + dw * (x + 1)), 0, (uint32_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<uint32_t>(offset, { (uint32_t)(-w2 + dw * (x + 1)), 0, (uint32_t)(-d2 + dh * (z + 1)) });
									offset += strideInBytes;

									setVertexData<uint32_t>(offset, { (uint32_t)(-w2 + dw * (x + 1)), 0, (uint32_t)(-d2 + dh * z) });
									offset += strideInBytes;

									setVertexData<uint32_t>(offset, { (uint32_t)(-w2 + dw * x), 0, (uint32_t)(-d2 + dh * z) });
									offset += strideInBytes;
								}
							}
						}
						break;

					default:
						THROW_MPP("Unsupported datatype: " + mesh::Vertex::getDataTypeName(attrib.dataType), __LINE__, __FILE__, __FUNCTION__);
					}
					break;

				case mesh::Vertex::Component::Position4:
					switch (attrib.dataType)
					{
					case mesh::Vertex::DataType::Float:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<float>(offset, { (float)(-w2 + dw * x), 0.0f, (float)(-d2 + dh * z), 1.0f });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<float>(offset, { (float)(-w2 + dw * x), 0.0f, (float)(-d2 + dh * z), 1.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(-w2 + dw * x), 0.0f, (float)(-d2 + dh * (z + 1)), 1.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(-w2 + dw * (x + 1)), 0.0f, (float)(-d2 + dh * (z + 1)), 1.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(-w2 + dw * (x + 1)), 0.0f, (float)(-d2 + dh * (z + 1)), 1.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(-w2 + dw * (x + 1)), 0.0f, (float)(-d2 + dh * z), 1.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(-w2 + dw * x), 0.0f, (float)(-d2 + dh * z), 1.0f });
									offset += strideInBytes;
								}
							}
						}
						break;

					case mesh::Vertex::DataType::HalfFloat:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<half_float::half>(offset, { (half_float::half)(-w2 + dw * x), (half_float::half)0, (half_float::half)(-d2 + dh * z), (half_float::half)0 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<half_float::half>(offset, { (half_float::half)(-w2 + dw * x), (half_float::half)0, (half_float::half)(-d2 + dh * z), (half_float::half)0 });
									offset += strideInBytes;

									setVertexData<half_float::half>(offset, { (half_float::half)(-w2 + dw * x), (half_float::half)0, (half_float::half)(-d2 + dh * (z + 1)), (half_float::half)0 });
									offset += strideInBytes;

									setVertexData<half_float::half>(offset, { (half_float::half)(-w2 + dw * (x + 1)), (half_float::half)0, (half_float::half)(-d2 + dh * (z + 1)), (half_float::half)0 });
									offset += strideInBytes;

									setVertexData<half_float::half>(offset, { (half_float::half)(-w2 + dw * (x + 1)), (half_float::half)0, (half_float::half)(-d2 + dh * (z + 1)), (half_float::half)0 });
									offset += strideInBytes;

									setVertexData<half_float::half>(offset, { (half_float::half)(-w2 + dw * (x + 1)), (half_float::half)0, (half_float::half)(-d2 + dh * z), (half_float::half)0 });
									offset += strideInBytes;

									setVertexData<half_float::half>(offset, { (half_float::half)(-w2 + dw * x), (half_float::half)0, (half_float::half)(-d2 + dh * z), (half_float::half)0 });
									offset += strideInBytes;
								}
							}
						}
						break;

					case mesh::Vertex::DataType::Byte:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<int8_t>(offset, { (int8_t)(-w2 + dw * x), 0, (int8_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<int8_t>(offset, { (int8_t)(-w2 + dw * x), 0, (int8_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;

									setVertexData<int8_t>(offset, { (int8_t)(-w2 + dw * x), 0, (int8_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<int8_t>(offset, { (int8_t)(-w2 + dw * (x + 1)), 0, (int8_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<int8_t>(offset, { (int8_t)(-w2 + dw * (x + 1)), 0, (int8_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<int8_t>(offset, { (int8_t)(-w2 + dw * (x + 1)), 0, (int8_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;

									setVertexData<int8_t>(offset, { (int8_t)(-w2 + dw * x), 0, (int8_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;
								}
							}
						}
						break;

					case mesh::Vertex::DataType::UnsignedByte:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint8_t>(offset, { (uint8_t)(-w2 + dw * x), 0, (uint8_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<uint8_t>(offset, { (uint8_t)(-w2 + dw * x), 0, (uint8_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;

									setVertexData<uint8_t>(offset, { (uint8_t)(-w2 + dw * x), 0, (uint8_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<uint8_t>(offset, { (uint8_t)(-w2 + dw * (x + 1)), 0, (uint8_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<uint8_t>(offset, { (uint8_t)(-w2 + dw * (x + 1)), 0, (uint8_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<uint8_t>(offset, { (uint8_t)(-w2 + dw * (x + 1)), 0, (uint8_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;

									setVertexData<uint8_t>(offset, { (uint8_t)(-w2 + dw * x), 0, (uint8_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;
								}
							}
						}
						break;

					case mesh::Vertex::DataType::Short:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<int16_t>(offset, { (int16_t)(-w2 + dw * x), 0, (int16_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<int16_t>(offset, { (int16_t)(-w2 + dw * x), 0, (int16_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;

									setVertexData<int16_t>(offset, { (int16_t)(-w2 + dw * x), 0, (int16_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<int16_t>(offset, { (int16_t)(-w2 + dw * (x + 1)), 0, (int16_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<int16_t>(offset, { (int16_t)(-w2 + dw * (x + 1)), 0, (int16_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<int16_t>(offset, { (int16_t)(-w2 + dw * (x + 1)), 0, (int16_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;

									setVertexData<int16_t>(offset, { (int16_t)(-w2 + dw * x), 0, (int16_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;
								}
							}
						}
						break;

					case mesh::Vertex::DataType::UnsignedShort:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint16_t>(offset, { (uint16_t)(-w2 + dw * x), 0, (uint16_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<uint16_t>(offset, { (uint16_t)(-w2 + dw * x), 0, (uint16_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;

									setVertexData<uint16_t>(offset, { (uint16_t)(-w2 + dw * x), 0, (uint16_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<uint16_t>(offset, { (uint16_t)(-w2 + dw * (x + 1)), 0, (uint16_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<uint16_t>(offset, { (uint16_t)(-w2 + dw * (x + 1)), 0, (uint16_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<uint16_t>(offset, { (uint16_t)(-w2 + dw * (x + 1)), 0, (uint16_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;

									setVertexData<uint16_t>(offset, { (uint16_t)(-w2 + dw * x), 0, (uint16_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;
								}
							}
						}
						break;

					case mesh::Vertex::DataType::Int:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<int32_t>(offset, { (int32_t)(-w2 + dw * x), 0, (int32_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<int32_t>(offset, { (int32_t)(-w2 + dw * x), 0, (int32_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;

									setVertexData<int32_t>(offset, { (int32_t)(-w2 + dw * x), 0, (int32_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<int32_t>(offset, { (int32_t)(-w2 + dw * (x + 1)), 0, (int32_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<int32_t>(offset, { (int32_t)(-w2 + dw * (x + 1)), 0, (int32_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<int32_t>(offset, { (int32_t)(-w2 + dw * (x + 1)), 0, (int32_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;

									setVertexData<int32_t>(offset, { (int32_t)(-w2 + dw * x), 0, (int32_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;
								}
							}
						}
						break;

					case mesh::Vertex::DataType::UnsignedInt:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint32_t>(offset, { (uint32_t)(-w2 + dw * x), 0, (uint32_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<uint32_t>(offset, { (uint32_t)(-w2 + dw * x), 0, (uint32_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;

									setVertexData<uint32_t>(offset, { (uint32_t)(-w2 + dw * x), 0, (uint32_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<uint32_t>(offset, { (uint32_t)(-w2 + dw * (x + 1)), 0, (uint32_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<uint32_t>(offset, { (uint32_t)(-w2 + dw * (x + 1)), 0, (uint32_t)(-d2 + dh * (z + 1)), 1 });
									offset += strideInBytes;

									setVertexData<uint32_t>(offset, { (uint32_t)(-w2 + dw * (x + 1)), 0, (uint32_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;

									setVertexData<uint32_t>(offset, { (uint32_t)(-w2 + dw * x), 0, (uint32_t)(-d2 + dh * z), 1 });
									offset += strideInBytes;
								}
							}
						}
						break;

					default:
						THROW_MPP("Unsupported datatype: " + mesh::Vertex::getDataTypeName(attrib.dataType), __LINE__, __FILE__, __FUNCTION__);
					}
					break;

				case mesh::Vertex::Component::Normal3:
					switch (attrib.dataType)
					{
					case mesh::Vertex::DataType::Float:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<float>(offset, { 0.0f, 1.0f, 0.0f });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<float>(offset, { 0.0f, 1.0f, 0.0f });
										offset += strideInBytes;
									}
								}
							}
						}
						break;

					case mesh::Vertex::DataType::Byte:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<int8_t>(offset, { 0, 1, 0 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<int8_t>(offset, { 0, 1, 0 });
										offset += strideInBytes;
									}
								}
							}
						}
						break;

					case mesh::Vertex::DataType::UnsignedByte:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint8_t>(offset, { 0, 1, 0 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<uint8_t>(offset, { 0, 1, 0 });
										offset += strideInBytes;
									}
								}
							}
						}
						break;

					case mesh::Vertex::DataType::Short:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<int16_t>(offset, { 0, 1, 0 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<int16_t>(offset, { 0, 1, 0 });
										offset += strideInBytes;
									}
								}
							}
						}
						break;

					case mesh::Vertex::DataType::UnsignedShort:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint16_t>(offset, { 0, 1, 0 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<uint16_t>(offset, { 0, 1, 0 });
										offset += strideInBytes;
									}
								}
							}
						}
						break;

					case mesh::Vertex::DataType::Int:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<int32_t>(offset, { 0, 1, 0 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<int32_t>(offset, { 0, 1, 0 });
										offset += strideInBytes;
									}
								}
							}
						}
						break;

					case mesh::Vertex::DataType::UnsignedInt:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint32_t>(offset, { 0, 1, 0 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<uint32_t>(offset, { 0, 1, 0 });
										offset += strideInBytes;
									}
								}
							}
						}
						break;

					default:
						THROW_MPP("Unsupported datatype: " + mesh::Vertex::getDataTypeName(attrib.dataType), __LINE__, __FILE__, __FUNCTION__);
					}
					break;

				case mesh::Vertex::Component::Normal4:
					switch (attrib.dataType)
					{
					case mesh::Vertex::DataType::Float:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<float>(offset, { 0.0f, 1.0f, 0.0f, 1.0f });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<float>(offset, { 0.0f, 1.0f, 0.0f, 1.0f });
										offset += strideInBytes;
									}
								}
							}
						}
						break;

					case mesh::Vertex::DataType::Byte:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<int8_t>(offset, { 0, 1, 0, 1 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<int8_t>(offset, { 0, 1, 0, 1 });
										offset += strideInBytes;
									}
								}
							}
						}
						break;

					case mesh::Vertex::DataType::UnsignedByte:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint8_t>(offset, { 0, 1, 0, 1 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<uint8_t>(offset, { 0, 1, 0, 1 });
										offset += strideInBytes;
									}
								}
							}
						}
						break;

					case mesh::Vertex::DataType::Short:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<int16_t>(offset, { 0, 1, 0, 1 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<int16_t>(offset, { 0, 1, 0, 1 });
										offset += strideInBytes;
									}
								}
							}
						}
						break;

					case mesh::Vertex::DataType::UnsignedShort:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint16_t>(offset, { 0, 1, 0, 1 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<uint16_t>(offset, { 0, 1, 0, 1 });
										offset += strideInBytes;
									}
								}
							}
						}
						break;

					case mesh::Vertex::DataType::Int:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<int32_t>(offset, { 0, 1, 0, 1 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<int32_t>(offset, { 0, 1, 0, 1 });
										offset += strideInBytes;
									}
								}
							}
						}
						break;

					case mesh::Vertex::DataType::UnsignedInt:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint32_t>(offset, { 0, 1, 0, 1 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<uint32_t>(offset, { 0, 1, 0, 1 });
										offset += strideInBytes;
									}
								}
							}
						}
						break;

					case mesh::Vertex::DataType::Int_2_10_10_10_REV:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								uint32_t xs = 0;
								uint32_t ys = 0;
								uint32_t zs = 0;
								uint32_t ws = 0;

								int32_t val = 
									ws << 31 | ((uint32_t)(1.0f + (ws << 1)) & 1) << 30 |
									zs << 29 | ((uint32_t)(0.0f * 511 + (zs << 9)) & 511) << 20 |
									ys << 19 | ((uint32_t)(1.0f * 511 + (ys << 9)) & 511) << 10 |
									xs <<  9 | ((uint32_t)(0.0f * 511 + (xs << 9)) & 511);

								if (meshSpec.verticesIndexed())
								{
									setVertexData<int32_t>(offset, { val });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<int32_t>(offset, { val });
										offset += strideInBytes;
									}
								}
							}
						}
						break;

					default:
						THROW_MPP("Unsupported datatype: " + mesh::Vertex::getDataTypeName(attrib.dataType), __LINE__, __FILE__, __FUNCTION__);
					}
					break;

				case mesh::Vertex::Component::TexCoord2:
					switch (attrib.dataType)
					{
					case mesh::Vertex::DataType::Float:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<float>(offset, { (float)x / dimX, (float)z / dimZ });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<float>(offset, { (float)x / dimX, (float)z / dimZ });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)x / dimX, (float)(z + 1) / dimZ });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(x + 1) / dimX, (float)(z + 1) / dimZ });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(x + 1) / dimX, (float)(z + 1) / dimZ });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(x + 1) / dimX, (float)z / dimZ });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)x / dimX, (float)z / dimZ });
									offset += strideInBytes;
								}
							}
						}
						break;

					default:
						THROW_MPP("Unsupported datatype: " + mesh::Vertex::getDataTypeName(attrib.dataType), __LINE__, __FILE__, __FUNCTION__);
					}
					break;

				case mesh::Vertex::Component::TexCoord3:
					switch (attrib.dataType)
					{
					case mesh::Vertex::DataType::Float:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<float>(offset, { (float)x / dimX, (float)z / dimZ, 0.0f });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<float>(offset, { (float)x / dimX, (float)z / dimZ, 0.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)x / dimX, (float)(z + 1) / dimZ, 0.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(x + 1) / dimX, (float)(z + 1) / dimZ, 0.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(x + 1) / dimX, (float)(z + 1) / dimZ, 0.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(x + 1) / dimX, (float)z / dimZ, 0.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)x / dimX, (float)z / dimZ, 0.0f });
									offset += strideInBytes;
								}
							}
						}
						break;

					default:
						THROW_MPP("Unsupported datatype: " + mesh::Vertex::getDataTypeName(attrib.dataType), __LINE__, __FILE__, __FUNCTION__);
					}
					break;

				case mesh::Vertex::Component::TexCoord4:
					switch (attrib.dataType)
					{
					case mesh::Vertex::DataType::Float:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<float>(offset, { (float)x / dimX, (float)z / dimZ, 0.0f, 0.0f });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<float>(offset, { (float)x / dimX, (float)z / dimZ, 0.0f, 0.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)x / dimX, (float)(z + 1) / dimZ, 0.0f, 0.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(x + 1) / dimX, (float)(z + 1) / dimZ, 0.0f, 0.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(x + 1) / dimX, (float)(z + 1) / dimZ, 0.0f, 0.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)(x + 1) / dimX, (float)z / dimZ, 0.0f, 0.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { (float)x / dimX, (float)z / dimZ, 0.0f, 0.0f });
									offset += strideInBytes;
								}
							}
						}
						break;

					default:
						THROW_MPP("Unsupported datatype: " + mesh::Vertex::getDataTypeName(attrib.dataType), __LINE__, __FILE__, __FUNCTION__);
					}
					break;

				case mesh::Vertex::Component::Colour3:
					for (int z = 0; z <= dimZ; ++z)
					{
						for (int x = 0; x <= dimX; ++x)
						{
							switch (attrib.dataType)
							{
							case mesh::Vertex::DataType::Float:
								if (meshSpec.verticesIndexed())
								{
									setVertexData<float>(offset, { 1.0f, 1.0f, 1.0f });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<float>(offset, { 1.0f, 1.0f, 1.0f });
										offset += strideInBytes;
									}
								}
								break;

							case mesh::Vertex::DataType::UnsignedByte:
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint8>(offset, { 0xff, 0xff, 0xff });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<uint8>(offset, { 0xff, 0xff, 0xff });
										offset += strideInBytes;
									}
								}
								break;

							case mesh::Vertex::DataType::UnsignedShort:
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint16>(offset, { 0xffff, 0xffff, 0xffff });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<uint16>(offset, { 0xffff, 0xffff, 0xffff });
										offset += strideInBytes;
									}
								}
								break;

							case mesh::Vertex::DataType::UnsignedInt:
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint32>(offset, { 0xffffffff, 0xffffffff, 0xffffffff });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<uint32>(offset, { 0xffffffff, 0xffffffff, 0xffffffff });
										offset += strideInBytes;
									}
								}
								break;

							default:
								THROW_MPP("Unsupported datatype: " + mesh::Vertex::getDataTypeName(attrib.dataType), __LINE__, __FILE__, __FUNCTION__);
							}
						}
					}
					break;

				case mesh::Vertex::Component::Colour4:
					for (int z = 0; z <= dimZ; ++z)
					{
						for (int x = 0; x <= dimX; ++x)
						{
							switch (attrib.dataType)
							{
							case mesh::Vertex::DataType::Float:
								if (meshSpec.verticesIndexed())
								{
									setVertexData<float>(offset, { 1.0f, 1.0f, 1.0f, 1.0f });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<float>(offset, { 1.0f, 1.0f, 1.0f, 1.0f });
										offset += strideInBytes;
									}
								}
								break;

							case mesh::Vertex::DataType::UnsignedByte:
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint8>(offset, { 0xff, 0xff, 0xff, 0xff });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<uint8>(offset, { 0xff, 0xff, 0xff, 0xff });
										offset += strideInBytes;
									}
								}
								break;

							case mesh::Vertex::DataType::UnsignedShort:
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint16>(offset, { 0xffff, 0xffff, 0xffff, 0xffff });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<uint16>(offset, { 0xffff, 0xffff, 0xffff, 0xffff });
										offset += strideInBytes;
									}
								}
								break;

							case mesh::Vertex::DataType::UnsignedInt:
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint32>(offset, { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<uint32>(offset, { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff });
										offset += strideInBytes;
									}
								}
								break;

							case mesh::Vertex::DataType::UnsignedInt_2_10_10_10_REV:
								if (meshSpec.verticesIndexed())
								{
									uint32_t val = (3 << 30) + (1023 << 20) + (1023 << 10) + (1023 << 0);
									setVertexData<uint32>(offset, { val  });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									uint32_t val = (3 << 30) + (1023 << 20) + (1023 << 10) + (1023 << 0);
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<uint32>(offset, { val });
										offset += strideInBytes;
									}
								}
								break;

							default:
								THROW_MPP("Unsupported datatype: " + mesh::Vertex::getDataTypeName(attrib.dataType), __LINE__, __FILE__, __FUNCTION__);
							}
						}
					}
					break;

				default:
					THROW_MPP("Unsupported datatype: " + mesh::Vertex::getComponentName(attrib.component), __LINE__, __FILE__, __FUNCTION__);
				}
			}
		}

		// Faces
		int unindexedCount = 0;
		for (int z = 0; z < dimZ; ++z)
		{
			for (int x = 0; x < dimX; ++x)
			{
				if (meshSpec.verticesIndexed())
				{
					int offset = (dimX + 1) * z + x;
					addTriangle(offset, offset + dimX + 1, offset + dimX + 2);
					addTriangle(offset + dimX + 2, offset + 1, offset);
				}
				else if (x != dimX && z != dimZ)
				{
					addTriangle(unindexedCount + 0, unindexedCount + 1, unindexedCount + 2);
					addTriangle(unindexedCount + 3, unindexedCount + 4, unindexedCount + 5);
					unindexedCount += 6;
				}
			}
		}
	}
}