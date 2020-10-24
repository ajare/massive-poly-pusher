#include <algorithm>
#include <cassert>

#include <half/half.hpp>

#include "mpp/Config.h"
#include "mpp/GridModelStream.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{
	GridModelStream::GridModelStream(ResourceManager* resourceMgr, mesh::MeshSpecification const& meshSpec, string const& material, double width, double depth, size_t dimX, size_t dimZ)
		: PrimitiveModelStream(resourceMgr, meshSpec, material)
	{
		// Spheres always use indexed vertices.
		mMeshDataDefinition.specification.setIndexedVertices(true);

		size_t strideInBytes;
		map<string, size_t> componentOffsets = getComponentOffsets(strideInBytes);

		// Preallocate vertex buffer
		const size_t numVertices = (dimX + 1) * (dimZ + 1);
		size_t bufferSize = strideInBytes * numVertices;

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
					for (size_t z = 0; z <= dimZ; ++z)
					{
						for (size_t x = 0; x <= dimX; ++x)
						{
							setData(offset, attrib.component, attrib.dataType, attrib.normalised, -w2 + dw * x, 0.0, -d2 + dh * z);
							offset += strideInBytes;
						}
					}
					break;
				case mesh::Vertex::Component::Normal3:
				case mesh::Vertex::Component::Normal4:
					for (size_t z = 0; z <= dimZ; ++z)
					{
						for (size_t x = 0; x <= dimX; ++x)
						{
							setData(offset, attrib.component, attrib.dataType, attrib.normalised, 0.0, 1.0, 0.0);
							offset += strideInBytes;
						}
					}
					break;
				case mesh::Vertex::Component::TexCoord2:
				case mesh::Vertex::Component::TexCoord3:
				case mesh::Vertex::Component::TexCoord4:
					for (size_t z = 0; z <= dimZ; ++z)
					{
						for (size_t x = 0; x <= dimX; ++x)
						{
							setData(offset, attrib.component, attrib.dataType, attrib.normalised, (double)x / dimX, (double)z / dimZ);
							offset += strideInBytes;
						}
					}
					break;
				case mesh::Vertex::Component::Colour1:
					for (size_t z = 0; z <= dimZ; ++z)
					{
						for (size_t x = 0; x <= dimX; ++x)
						{
							setData(offset, attrib.component, attrib.dataType, attrib.normalised, 1.0);
							offset += strideInBytes;
						}
					}
					break;
				case mesh::Vertex::Component::Colour3:
				case mesh::Vertex::Component::Colour4:
					for (size_t z = 0; z <= dimZ; ++z)
					{
						for (size_t x = 0; x <= dimX; ++x)
						{
							setData(offset, attrib.component, attrib.dataType, attrib.normalised, 1.0, 1.0, 1.0);
							offset += strideInBytes;
						}
					}
					break;
				}
			}
		}

		// Faces
		for (size_t z = 0; z < dimZ; ++z)
		{
			for (size_t x = 0; x < dimX; ++x)
			{
				size_t offset = (dimX + 1) * z + x;
				addTriangle(offset, offset + dimX + 1, offset + dimX + 2);
				addTriangle(offset + dimX + 2, offset + 1, offset);
			}
		}
	}
}