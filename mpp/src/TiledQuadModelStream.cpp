#include <algorithm>
#include <cassert>

#include "mpp/Config.h"
#include "mpp/TiledQuadModelStream.h"

using namespace std;

namespace mpp
{
	TiledQuadModelStream::TiledQuadModelStream(mesh::MeshSpecification const& meshSpec, string const& material, float width, float depth)
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
		int verticesPerFace = (meshSpec.verticesIndexed() ? 4 : 6);
		const int numVertices = verticesPerFace * 1;
		int bufferSize = strideInBytes * numVertices;

		mMeshDataDefinition.vertexData.resize(bufferSize);

		// Generate vertices
		float w2 = width / 2;
		float d2 = depth / 2;

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
						setVertexData<float>(offset, { -w2, 0, -d2 });	offset += strideInBytes;
						setVertexData<float>(offset, { -w2, 0, d2 }); offset += strideInBytes;
						setVertexData<float>(offset, { w2, 0, d2 }); offset += strideInBytes;
						if (!meshSpec.verticesIndexed())
						{
							setVertexData<float>(offset, { w2, 0, d2 }); offset += strideInBytes;
						}
						setVertexData<float>(offset, { w2, 0, -d2 }); offset += strideInBytes;
						if (!meshSpec.verticesIndexed())
						{
							setVertexData<float>(offset, { -w2, 0, -d2 });	offset += strideInBytes;
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
						setVertexData<float>(offset, { 0, 1, 0 });	offset += strideInBytes;
						setVertexData<float>(offset, { 0, 1, 0 }); offset += strideInBytes;
						setVertexData<float>(offset, { 0, 1, 0 }); offset += strideInBytes;
						if (!meshSpec.verticesIndexed())
						{
							setVertexData<float>(offset, { 0, 1, 0 }); offset += strideInBytes;
						}
						setVertexData<float>(offset, { 0, 1, 0 }); offset += strideInBytes;
						if (!meshSpec.verticesIndexed())
						{
							setVertexData<float>(offset, { 0, 1, 0 }); offset += strideInBytes;
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
						setVertexData<float>(offset, { 0, 0 }); offset += strideInBytes;
						setVertexData<float>(offset, { 0, 1 }); offset += strideInBytes;
						setVertexData<float>(offset, { 1, 1 }); offset += strideInBytes;
						if (!meshSpec.verticesIndexed())
						{
							setVertexData<float>(offset, { 1, 1 }); offset += strideInBytes;
						}
						setVertexData<float>(offset, { 1, 0 }); offset += strideInBytes;
						if (!meshSpec.verticesIndexed())
						{
							setVertexData<float>(offset, { 0, 0 }); offset += strideInBytes;
						}
						break;

					default:
						throw exception("Primitive ModelStreams only support floats for texcoord data.");
					}
					break;

				case mesh::Vertex::Component::Colour3:
					switch (attrib.dataType)
					{
					case mesh::Vertex::DataType::Float:
						setVertexData<float>(offset, { 1, 1, 1 }); offset += strideInBytes;
						setVertexData<float>(offset, { 1, 1, 1 }); offset += strideInBytes;
						setVertexData<float>(offset, { 1, 1, 1 }); offset += strideInBytes;
						if (!meshSpec.verticesIndexed())
						{
							setVertexData<float>(offset, { 1, 1, 1 }); offset += strideInBytes;
						}
						setVertexData<float>(offset, { 1, 1, 1 }); offset += strideInBytes;
						if (!meshSpec.verticesIndexed())
						{
							setVertexData<float>(offset, { 1, 1, 1 }); offset += strideInBytes;
						}
						break;

					case mesh::Vertex::DataType::UnsignedByte:
						setVertexData<float>(offset, { 255, 255, 255 }); offset += strideInBytes;
						setVertexData<float>(offset, { 255, 255, 255 }); offset += strideInBytes;
						setVertexData<float>(offset, { 255, 255, 255 }); offset += strideInBytes;
						if (!meshSpec.verticesIndexed())
						{
							setVertexData<float>(offset, { 255, 255, 255 }); offset += strideInBytes;
						}
						setVertexData<float>(offset, { 255, 255, 255 }); offset += strideInBytes;
						if (!meshSpec.verticesIndexed())
						{
							setVertexData<float>(offset, { 255, 255, 255 }); offset += strideInBytes;
						}
						break;

					default:
						throw exception("Primitive ModelStreams only support floats or ubytes for colour data.");
					}
					break;

				case mesh::Vertex::Component::Colour4:
					switch (attrib.dataType)
					{
					case mesh::Vertex::DataType::Float:
						setVertexData<float>(offset, { 1, 1, 1, 1 }); offset += strideInBytes;
						setVertexData<float>(offset, { 1, 1, 1, 1 }); offset += strideInBytes;
						setVertexData<float>(offset, { 1, 1, 1, 1 }); offset += strideInBytes;
						if (!meshSpec.verticesIndexed())
						{
							setVertexData<float>(offset, { 1, 1, 1, 1 }); offset += strideInBytes;
						}
						setVertexData<float>(offset, { 1, 1, 1, 1 }); offset += strideInBytes;
						if (!meshSpec.verticesIndexed())
						{
							setVertexData<float>(offset, { 1, 1, 1, 1 }); offset += strideInBytes;
						}
						break;

					case mesh::Vertex::DataType::UnsignedByte:
						setVertexData<float>(offset, { 255, 255, 255, 255 }); offset += strideInBytes;
						setVertexData<float>(offset, { 255, 255, 255, 255 }); offset += strideInBytes;
						setVertexData<float>(offset, { 255, 255, 255, 255 }); offset += strideInBytes;
						if (!meshSpec.verticesIndexed())
						{
							setVertexData<float>(offset, { 255, 255, 255, 255 }); offset += strideInBytes;
						}
						setVertexData<float>(offset, { 255, 255, 255, 255 }); offset += strideInBytes;
						if (!meshSpec.verticesIndexed())
						{
							setVertexData<float>(offset, { 255, 255, 255, 255 }); offset += strideInBytes;
						}
						break;

					default:
						throw exception("Primitive ModelStreams only support floats or ubytes for colour data.");
					}
					break;

				default:
					throw exception("Unsupported component.");

				}
			}
		}

		for (int i = 0; i < numVertices; i += verticesPerFace)
		{
			addTriangle(i + 0, i + 1, i + 2);
			if (meshSpec.verticesIndexed())
				addTriangle(i + 2, i + 3, i + 0);
			else
				addTriangle(i + 3, i + 4, i + 5);
		}
	}
}