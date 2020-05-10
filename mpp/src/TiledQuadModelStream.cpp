#include <algorithm>
#include <cassert>

#include "mpp/Config.h"
#include "mpp/TiledQuadModelStream.h"

using namespace std;

namespace mpp
{
	TiledQuadModelStream::TiledQuadModelStream(mesh::MeshSpecification const& meshSpec, string const& material, float width, float depth, int dimX, int dimZ)
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
								setVertexData<float>(offset, { -w2 + dw * x, 0, -d2 + dh * z });
								offset += strideInBytes;
							}
						}
						break;

					default:
						throw exception("Primitive ModelStreams only support floats for position data.");
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
								setVertexData<float>(offset, { 0, 1, 0 });
								offset += strideInBytes;
							}
						}
						break;

					default:
						throw exception("Primitive ModelStreams only support floats for normal data.");
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
								setVertexData<float>(offset, { (float)x / dimX, (float)z / dimZ });
								offset += strideInBytes;
							}
						}
						break;

					default:
						throw exception("Primitive ModelStreams only support floats for texcoord data.");
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
								setVertexData<float>(offset, { 1, 1, 1 });
								break;

							case mesh::Vertex::DataType::UnsignedByte:
								setVertexData<float>(offset, { 255, 255, 255 });
								break;

							default:
								throw exception("Primitive ModelStreams only support floats or ubytes for colour data.");
							}

							offset += strideInBytes;
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
								setVertexData<float>(offset, { 1, 1, 1, 1});
								break;

							case mesh::Vertex::DataType::UnsignedByte:
								setVertexData<float>(offset, { 255, 255, 255, 255 });
								break;

							default:
								throw exception("Primitive ModelStreams only support floats or ubytes for colour data.");
							}

							offset += strideInBytes;
						}
					}
					break;

				default:
					throw exception("Unsupported component.");
				}
			}
		}

		// Faces
		for (int z = 0; z < dimZ; ++z)
		{
			for (int x = 0; x < dimX; ++x)
			{
				int offset = (dimX + 1) * z + x;
				addTriangle(offset, offset + dimX + 1, offset + dimX + 2);
				addTriangle(offset + dimX + 2, offset + 1, offset);
			}
		}
	}
}