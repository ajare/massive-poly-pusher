#include <algorithm>
#include <cassert>

#include "mpp/Config.h"
#include "mpp/CylinderModelStream.h"

using namespace std;

namespace mpp
{
	CylinderModelStream::CylinderModelStream(ResourceManager* resourceMgr, mesh::MeshSpecification const& meshSpec, string const& material, float length, float radius1, float radius2, int res)
		: PrimitiveModelStream(resourceMgr, meshSpec, material)
		, mResolution(res)
	{
		// Cylinders always use indexed vertices.
		mMeshDataDefinition.specification.setIndexedVertices(true);

		size_t strideInBytes;
		map<string, size_t> componentOffsets = getComponentOffsets(strideInBytes);

		// Preallocate vertex buffer
		const int numVertices = (res + 1) * 2 + 2; // Two extra for caps centres.
		int bufferSize = strideInBytes * numVertices;

		mMeshDataDefinition.vertexData.resize(bufferSize);

		// Generate vertices
		double l2 = length / 2;

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
					// Top cap
					setData(offset, attrib.component, attrib.dataType, attrib.normalised, 0.0, l2, 0.0);
					offset += strideInBytes;
					
					// Bottom cap
					setData(offset, attrib.component, attrib.dataType, attrib.normalised, 0.0, -l2, 0.0);
					offset += strideInBytes;

					// Sides
					for (int i = 0; i <= res; ++i)
					{
						float nx, nz, x1, x2, z1, z2, angleInc = 2 * 3.14159f / res;

						nx = sin(angleInc * i);
						nz = cos(angleInc * i);

						x1 = nx * radius1;
						z1 = nz * radius1;

						x2 = nx * radius2;
						z2 = nz * radius2;

						setData(offset, attrib.component, attrib.dataType, attrib.normalised, x1, l2, z1);
						offset += strideInBytes;

						setData(offset, attrib.component, attrib.dataType, attrib.normalised, x2, -l2, z2);
						offset += strideInBytes;
					}
					break;
				case mesh::Vertex::Component::Normal3:
				case mesh::Vertex::Component::Normal4:
					// Top cap
					setData(offset, attrib.component, attrib.dataType, attrib.normalised, 0.0, 1.0, 0.0);
					offset += strideInBytes;

					// Top cap
					setData(offset, attrib.component, attrib.dataType, attrib.normalised, 0.0, -1.0, 0.0);
					offset += strideInBytes;

					// Sides
					for (int i = 0; i <= res; ++i)
					{
						float nx, nz, angleInc = 2 * 3.14159f / res;

						nx = sin(angleInc * i);
						nz = cos(angleInc * i);

						setData(offset, attrib.component, attrib.dataType, attrib.normalised, nx, 0, nz);
						offset += strideInBytes;

						setData(offset, attrib.component, attrib.dataType, attrib.normalised, nx, 0, nz);
						offset += strideInBytes;
					}
					break;
				case mesh::Vertex::Component::TexCoord2:
				case mesh::Vertex::Component::TexCoord3:
				case mesh::Vertex::Component::TexCoord4:
					// Top cap
					setData(offset, attrib.component, attrib.dataType, attrib.normalised, 0.5, 0.5);
					offset += strideInBytes;

					// Bottom cap
					setData(offset, attrib.component, attrib.dataType, attrib.normalised, 0.5, 0.5);
					offset += strideInBytes;

					// Sides
					for (int i = 0; i <= res; ++i)
					{
						setData(offset, attrib.component, attrib.dataType, attrib.normalised, i / (double)res, 1);
						offset += strideInBytes;

						setData(offset, attrib.component, attrib.dataType, attrib.normalised, i / (double)res, 0);
						offset += strideInBytes;
					}
					break;
				case mesh::Vertex::Component::Colour1:
					// Top cap
					setData(offset, attrib.component, attrib.dataType, attrib.normalised, 1.0);
					offset += strideInBytes;

					// Bottom cap
					setData(offset, attrib.component, attrib.dataType, attrib.normalised, 1.0);
					offset += strideInBytes;

					// Sides
					for (int i = 0; i <= res; ++i)
					{
						setData(offset, attrib.component, attrib.dataType, attrib.normalised, 1.0);
						offset += strideInBytes;

						setData(offset, attrib.component, attrib.dataType, attrib.normalised, 1.0);
						offset += strideInBytes;
					}
					break;
				case mesh::Vertex::Component::Colour3:
				case mesh::Vertex::Component::Colour4:
					// Top cap
					setData(offset, attrib.component, attrib.dataType, attrib.normalised, 1.0, 1.0, 1.0);
					offset += strideInBytes;

					// Bottom cap
					setData(offset, attrib.component, attrib.dataType, attrib.normalised, 1.0, 1.0, 1.0);
					offset += strideInBytes;

					// Sides
					for (int i = 0; i <= res; ++i)
					{
						setData(offset, attrib.component, attrib.dataType, attrib.normalised, 1.0, 1.0, 1.0);
						offset += strideInBytes;

						setData(offset, attrib.component, attrib.dataType, attrib.normalised, 1.0, 1.0, 1.0);
						offset += strideInBytes;
					}
					break;
				}
			}
		}
		
		// Top indices
		for (int i = 0; i < res; ++i)
		{
			addTriangle(0, i * 2 + 2, i * 2 + 4);
		}

		// Bottom indices
		for (int i = 0; i < res; ++i)
		{
			addTriangle(1, i * 2 + 3, i * 2 + 5);
		}

		// Side indices
		for (int i = 0; i < res; ++i)
		{
			uint32 v23 = i * 2 + 3;
			uint32 v31 = i * 2 + 4;
			
			addTriangle(i * 2 + 2, v23, v31);
			addTriangle(v31, i * 2 + 5, v23);
		}
	}
}