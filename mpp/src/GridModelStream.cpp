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
				case mesh::Vertex::Component::Position4:
					for (int z = 0; z <= dimZ; ++z)
					{
						for (int x = 0; x <= dimX; ++x)
						{
							if (meshSpec.verticesIndexed())
							{
								setData(offset, attrib.component, attrib.dataType, attrib.normalised, -w2 + dw * x, 0.0, -d2 + dh * z);
								offset += strideInBytes;
							}
							else if (x != dimX && z != dimZ)
							{
								setData(offset, attrib.component, attrib.dataType, attrib.normalised, -w2 + dw * x, 0.0, -d2 + dh * z);
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, -w2 + dw * x, 0.0, -d2 + dh * (z + 1));
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, -w2 + dw * (x + 1), 0.0, -d2 + dh * (z + 1));
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, -w2 + dw * (x + 1), 0.0, -d2 + dh * (z + 1));
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, -w2 + dw * (x + 1), 0.0, -d2 + dh * z);
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, -w2 + dw * x, 0.0, -d2 + dh * z);
								offset += strideInBytes;
							}
						}
					}
					break;
				case mesh::Vertex::Component::Normal3:
				case mesh::Vertex::Component::Normal4:
					for (int z = 0; z <= dimZ; ++z)
					{
						for (int x = 0; x <= dimX; ++x)
						{
							if (meshSpec.verticesIndexed())
							{
								setData(offset, attrib.component, attrib.dataType, attrib.normalised, 0.0, 1.0, 0.0);
								offset += strideInBytes;
							}
							else if (x != dimX && z != dimZ)
							{
								setData(offset, attrib.component, attrib.dataType, attrib.normalised, 0.0, 1.0, 0.0);
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, 0.0, 1.0, 0.0);
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, 0.0, 1.0, 0.0);
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, 0.0, 1.0, 0.0);
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, 0.0, 1.0, 0.0);
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, 0.0, 1.0, 0.0);
								offset += strideInBytes;
							}
						}
					}
					break;
				case mesh::Vertex::Component::TexCoord2:
				case mesh::Vertex::Component::TexCoord3:
				case mesh::Vertex::Component::TexCoord4:
					for (int z = 0; z <= dimZ; ++z)
					{
						for (int x = 0; x <= dimX; ++x)
						{
							if (meshSpec.verticesIndexed())
							{
								setData(offset, attrib.component, attrib.dataType, attrib.normalised, (double)x / dimX, (double)z / dimZ);
								offset += strideInBytes;
							}
							else if (x != dimX && z != dimZ)
							{
								setData(offset, attrib.component, attrib.dataType, attrib.normalised, (double)x / dimX, (double)z / dimZ);
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, (double)x / dimX, (double)(z + 1) / dimZ);
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, (double)(x + 1) / dimX, (double)(z + 1) / dimZ);
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, (double)(x + 1) / dimX, (double)(z + 1) / dimZ);
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, (double)(x + 1) / dimX, (double)z / dimZ);
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, (double)x / dimX, (double)z / dimZ);
								offset += strideInBytes;
							}
						}
					}
					break;
				case mesh::Vertex::Component::Colour1:
				case mesh::Vertex::Component::Colour3:
				case mesh::Vertex::Component::Colour4:
					for (int z = 0; z <= dimZ; ++z)
					{
						for (int x = 0; x <= dimX; ++x)
						{
							if (meshSpec.verticesIndexed())
							{
								setData(offset, attrib.component, attrib.dataType, attrib.normalised, 1.0, 1.0, 1.0);
								offset += strideInBytes;
							}
							else if (x != dimX && z != dimZ)
							{
								setData(offset, attrib.component, attrib.dataType, attrib.normalised, 1.0, 1.0, 1.0);
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, 1.0, 1.0, 1.0);
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, 1.0, 1.0, 1.0);
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, 1.0, 1.0, 1.0);
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, 1.0, 1.0, 1.0);
								offset += strideInBytes;

								setData(offset, attrib.component, attrib.dataType, attrib.normalised, 1.0, 1.0, 1.0);
								offset += strideInBytes;
							}
						}
					}
					break;
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