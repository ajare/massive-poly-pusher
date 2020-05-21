#include <algorithm>
#include <cassert>

#include "mpp/Config.h"
#include "mpp/GridModelStream.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{
	GridModelStream::GridModelStream(mesh::MeshSpecification const& meshSpec, string const& material, float width, float depth, int dimX, int dimZ)
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

				int componentSize = mesh::Vertex::getComponentSize(attrib.component) * mesh::Vertex::getDataTypeSize(attrib.dataType);

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
		float w2 = width / 2;
		float d2 = depth / 2;
		float dw = width / dimX;
		float dh = depth / dimZ;

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
					case mesh::Vertex::DataType::Float:
						for (int z = 0; z <= dimZ; ++z)
						{
							for (int x = 0; x <= dimX; ++x)
							{
								if (meshSpec.verticesIndexed())
								{
									setVertexData<float>(offset, { -w2 + dw * x, 0, -d2 + dh * z });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<float>(offset, { -w2 + dw * x, 0, -d2 + dh * z });
									offset += strideInBytes;

									setVertexData<float>(offset, { -w2 + dw * x, 0, -d2 + dh * (z + 1) });
									offset += strideInBytes;

									setVertexData<float>(offset, { -w2 + dw * (x + 1), 0, -d2 + dh * (z + 1) });
									offset += strideInBytes;

									setVertexData<float>(offset, { -w2 + dw * (x + 1), 0, -d2 + dh * (z + 1) });
									offset += strideInBytes;

									setVertexData<float>(offset, { -w2 + dw * (x + 1), 0, -d2 + dh * z });
									offset += strideInBytes;

									setVertexData<float>(offset, { -w2 + dw * x, 0, -d2 + dh * z });
									offset += strideInBytes;
								}
							}
						}
						break;

					default:
						THROW_MPP("Primitive ModelStreams only support floats for position data.", __LINE__, __FILE__, __FUNCTION__);
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
									setVertexData<float>(offset, { -w2 + dw * x, 0, -d2 + dh * z, 1.0f });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									setVertexData<float>(offset, { -w2 + dw * x, 0, -d2 + dh * z, 1.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { -w2 + dw * x, 0, -d2 + dh * (z + 1), 1.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { -w2 + dw * (x + 1), 0, -d2 + dh * (z + 1), 1.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { -w2 + dw * (x + 1), 0, -d2 + dh * (z + 1), 1.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { -w2 + dw * (x + 1), 0, -d2 + dh * z, 1.0f });
									offset += strideInBytes;

									setVertexData<float>(offset, { -w2 + dw * x, 0, -d2 + dh * z, 1.0f });
									offset += strideInBytes;
								}
							}
						}
						break;

					default:
						THROW_MPP("Primitive ModelStreams only support floats for position data.", __LINE__, __FILE__, __FUNCTION__);
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
									setVertexData<float>(offset, { 0, 1, 0 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<float>(offset, { 0, 1, 0 });
										offset += strideInBytes;
									}
								}
							}
						}
						break;

					default:
						THROW_MPP("Primitive ModelStreams only support floats for normal data.", __LINE__, __FILE__, __FUNCTION__);
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
						THROW_MPP("Primitive ModelStreams only support floats for texcoord data.", __LINE__, __FILE__, __FUNCTION__);
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
						THROW_MPP("Primitive ModelStreams only support floats for texcoord data.", __LINE__, __FILE__, __FUNCTION__);
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
						THROW_MPP("Primitive ModelStreams only support floats for texcoord data.", __LINE__, __FILE__, __FUNCTION__);
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
									setVertexData<float>(offset, { 1, 1, 1 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<float>(offset, { 1, 1, 1 });
										offset += strideInBytes;
									}
								}
								break;

							case mesh::Vertex::DataType::UnsignedByte:
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint8>(offset, { 255, 255, 255 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<uint8>(offset, { 255, 255, 255 });
										offset += strideInBytes;
									}
								}
								break;

							default:
								THROW_MPP("Primitive ModelStreams only support floats or ubytes for colour data.", __LINE__, __FILE__, __FUNCTION__);
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
									setVertexData<float>(offset, { 1, 1, 1, 1 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<float>(offset, { 1, 1, 1, 1 });
										offset += strideInBytes;
									}
								}
								break;

							case mesh::Vertex::DataType::UnsignedByte:
								if (meshSpec.verticesIndexed())
								{
									setVertexData<uint8>(offset, { 255, 255, 255, 255 });
									offset += strideInBytes;
								}
								else if (x != dimX && z != dimZ)
								{
									for (int k = 0; k < 6; ++k)
									{
										setVertexData<uint8>(offset, { 255, 255, 255, 255 });
										offset += strideInBytes;
									}
								}
								break;

							default:
								THROW_MPP("Primitive ModelStreams only support floats or ubytes for colour data.", __LINE__, __FILE__, __FUNCTION__);
							}
						}
					}
					break;

				default:
					THROW_MPP("Unsupported component.", __LINE__, __FILE__, __FUNCTION__);
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