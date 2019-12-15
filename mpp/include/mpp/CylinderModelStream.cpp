#include <algorithm>
#include <cassert>

#include "mpp/Config.h"
#include "mpp/CylinderModelStream.h"

using namespace std;

namespace mpp
{
	CylinderModelStream::CylinderModelStream(mesh::MeshSpecification const& meshSpec, string const& material, float length, float radius1, float radius2, int res)
		: PrimitiveModelStream(meshSpec, material)
		, mResolution(res)
	{
		// Cylinders always use indexed vertices.
		mMeshDataDefinition.specification.setIndexedVertices(true);

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

		const int numVertices = res * 2 + 2; // 2 extra vertices for the centres of the caps.
		int bufferSize = strideInBytes * numVertices / sizeof(float);

		mMeshDataDefinition.vertexData.resize(bufferSize);

		// Generate vertices
		float l2 = length / 2;

		// Top cap
		int offset = 0;
		setVertexData<float>(offset + 0, { 0, l2, 0 });
		setVertexData<float>(offset + 12, { 0, 1, 0 });
		setVertexData<float>(offset + 24, { 0.5f, 0.5f });
		setVertexData<float>(offset + 32, { 1, 1, 1, 1 });
		offset += strideInBytes;

		// Bottom cap
		setVertexData<float>(offset + 0, { 0, -l2, 0 });
		setVertexData<float>(offset + 12, { 0, -1, 0 });
		setVertexData<float>(offset + 24, { 0.5f, 0.5f });
		setVertexData<float>(offset + 32, { 1, 1, 1, 1 });
		offset += strideInBytes;

		float angle = 0.0f, angleInc = sin(2 * 3.14159f / res);
		for (int i = 0; i < res; ++i)
		{
			float nx = sin(angle);
			float nz = cos(angle);

			float d = sqrt(nx * nx + nz * nz);
			nx /= d;
			nz /= d;

			float x1 = nx * radius1;
			float z1 = nz * radius1;

			float x2 = nx * radius2;
			float z2 = nz * radius2;

			// Top vertices
			setVertexData<float>(offset + 0, { x1, l2, z1 });
			setVertexData<float>(offset + 12, { nx, 0, nz });
			setVertexData<float>(offset + 24, { i / (float)res, 1 });
			setVertexData<float>(offset + 32, { 1, 1, 1, 1 });
			offset += strideInBytes;

			// Bottom vertices
			setVertexData<float>(offset + 0, { x2, -l2, z2 });
			setVertexData<float>(offset + 12, { nx, 0, nz });
			setVertexData<float>(offset + 24, { i / (float)res, 0 });
			setVertexData<float>(offset + 32, { 1, 1, 1, 1 });
			offset += strideInBytes;
		}

		// Top indices
		for (int i = 0; i < res; ++i)
		{
			addTriangle(0, i * 2 + 2, i == (res - 1) ? 2 : (i * 2 + 4));
		}

		// Bottom indices
		for (int i = 0; i < res; ++i)
		{
			addTriangle(1, i * 2 + 3, i == (res - 1) ? 3 : (i * 2 + 5));
		}

		// Side indices
		for (int i = 0; i < res; ++i)
		{
			uint32 v23 = i * 2 + 3;
			uint32 v31 = i == (res - 1) ? 2 : (i * 2 + 4);
			
			addTriangle(i * 2 + 2, v23, v31);
			addTriangle(v31, i == (res - 1) ? 3 : (i * 2 + 5), v23);
		}

	}
}