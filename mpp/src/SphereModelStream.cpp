#include <algorithm>
#include <cassert>

#include "mpp/Config.h"
#include "mpp/SphereModelStream.h"

using namespace std;

namespace mpp
{
	SphereModelStream::SphereModelStream(mesh::MeshSpecification const& meshSpec, string const& material, float radius, int res)
		: PrimitiveModelStream(meshSpec, material)
		, mRadius(radius)
		, mResolution(res)
	{
		// Spheres always use indexed vertices.
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

		const int numVertices = 12;
		int bufferSize = strideInBytes * numVertices / sizeof(float);

		mMeshDataDefinition.vertexData.resize(bufferSize);

		// Generate vertices
		float x = 0.525731112119133606f;
		float z = 0.850650808352039932f;

		vector<float> positions = 
		{ 
			-x, 0, z,
			x, 0, z,
			-x, 0, -z,
			x, 0, -z,
			0, z, x,
			0, z, -x,
			0, -z, x,
			0, -z, -x,
			z, x, 0,
			-z, x, 0,
			z, -x, 0,
			-z, -x, 0 
		};

		for (int i = 0, offset = 0; i < (int)positions.size(); i += 3, offset += strideInBytes)
		{
			// Position
			setVertexData<float>(offset + 0, { positions[i], positions[i + 1], positions[i + 2] });
			
			// Normal
			setVertexData<float>(offset + 12, { positions[i], positions[i + 1], positions[i + 2] });
			
			// UV coords
			float u, v;
			getUvCoord(positions[i], positions[i + 1], positions[i + 2], &u, &v);
			setVertexData<float>(offset + 24, { u, v });

			// Colour
			setVertexData<float>(offset + 32, { 1, 1, 1, 1 });
		}

		// Triangle indices
		addTriangle(1, 4, 0);
		addTriangle(4, 9, 0);
		addTriangle(4, 5, 9);
		addTriangle(8, 5, 4);
		addTriangle(1, 8, 4);
		addTriangle(1, 10, 8);
		addTriangle(10, 3, 8);
		addTriangle(8, 3, 5);
		addTriangle(3, 2, 5);
		addTriangle(3, 7, 2);
		addTriangle(3, 10, 7);
		addTriangle(10, 6, 7);
		addTriangle(6, 11, 7);
		addTriangle(6, 0, 11);
		addTriangle(6, 1, 0);
		addTriangle(10, 1, 6);
		addTriangle(11, 0, 9);
		addTriangle(2, 11, 9);
		addTriangle(5, 2, 9);
		addTriangle(11, 2, 7);

		// Resize data here, to stop this happening in subdivide
		size_t scaleFactor = pow(4, (res - 1));
		//mMeshDataDefinition.vertexData.resize(mMeshDataDefinition.vertexData.size() * scaleFactor);
		//mMeshDataDefinition.indexData.resize(mMeshDataDefinition.indexData.size() * scaleFactor);
		for (int i = 1; i < res; ++i)
		{
			subdivide(strideInBytes / sizeof(float));
		}

		renormalise(strideInBytes / sizeof(float));
	}

	void SphereModelStream::subdivide(int vertexStride)
	{
		map<uint64, uint32> midpointIndices;

		int indicesToProcess = (int)mMeshDataDefinition.indexData.size();
		for (int i = 0; i < indicesToProcess; i += 3)
		{
			uint32 i0 = mMeshDataDefinition.indexData[i + 0];
			uint32 i1 = mMeshDataDefinition.indexData[i + 1];
			uint32 i2 = mMeshDataDefinition.indexData[i + 2];

			uint32 m01 = getMidpointIndex(midpointIndices, mMeshDataDefinition.vertexData, vertexStride, i0, i1);
			uint32 m12 = getMidpointIndex(midpointIndices, mMeshDataDefinition.vertexData, vertexStride, i1, i2);
			uint32 m20 = getMidpointIndex(midpointIndices, mMeshDataDefinition.vertexData, vertexStride, i2, i0);

			// Add new indices
			mMeshDataDefinition.indexData[i + 0] = i0;
			mMeshDataDefinition.indexData[i + 1] = m01;
			mMeshDataDefinition.indexData[i + 2] = m20;

			mMeshDataDefinition.indexData.push_back(i1);
			mMeshDataDefinition.indexData.push_back(m12);
			mMeshDataDefinition.indexData.push_back(m01);

			mMeshDataDefinition.indexData.push_back(i2);
			mMeshDataDefinition.indexData.push_back(m20);
			mMeshDataDefinition.indexData.push_back(m12);

			mMeshDataDefinition.indexData.push_back(m20);
			mMeshDataDefinition.indexData.push_back(m01);
			mMeshDataDefinition.indexData.push_back(m12);
		}
	}

	uint32 SphereModelStream::getMidpointIndex(map<uint64, uint32>& midpointIndices, vector<float>& vertexData, int vertexStride, uint32 i0, uint32 i1)
	{
		uint64 key = min<uint64>(i0, i1) << 32;
		key |= max(i0, i1);

		auto it = midpointIndices.find(key);
		if (it == midpointIndices.end())
		{
			uint32 index = vertexData.size() / vertexStride;
			midpointIndices[key] = index;

			// Construct new vertex
			for (int i = 0; i < vertexStride; ++i)
			{
				float v0 = vertexData[vertexStride * i0 + i];
				float v1 = vertexData[vertexStride * i1 + i];
				vertexData.push_back(v0 + (v1 - v0) * 0.5f);
			}

			return index;
		}
		else
		{
			return it->second;
		}
	}

	void SphereModelStream::renormalise(int vertexStride)
	{
		for (int i = 0; i < (int)mMeshDataDefinition.vertexData.size(); i += vertexStride)
		{
			float x = mMeshDataDefinition.vertexData[i + 0];
			float y = mMeshDataDefinition.vertexData[i + 1];
			float z = mMeshDataDefinition.vertexData[i + 2];

			float d = mRadius / sqrt(x * x + y * y + z * z);
			x *= d;
			y *= d;
			z *= d;

			mMeshDataDefinition.vertexData[i + 0] = x;
			mMeshDataDefinition.vertexData[i + 1] = y;
			mMeshDataDefinition.vertexData[i + 2] = z;
		}
	}

	void SphereModelStream::getUvCoord(float nx, float ny, float nz, float* u, float* v)
	{
		float normalisedX = 0;
		float normalisedZ = -1;

		if (((nx * nx) + (nz * nz)) > 0)
		{
			normalisedX = sqrt((nx * nx) / ((nx * nx) + (nz * nz)));
			if (nx < 0)
			{
				normalisedX = -normalisedX;
			}
			normalisedZ = sqrt((nz * nz) / ((nx * nx) + (nz * nz)));
			if (nz < 0)
			{
				normalisedZ = -normalisedZ;
			}
		}
		if (normalisedZ == 0)
		{
			*u = ((normalisedX * 3.14159f) / 2);
		}
		else 
		{
			*u = atan(normalisedX / normalisedZ);
			if (normalisedZ < 0)
			{
				*u += 3.14159f;
			}
			if (*u < 0)
			{
				*u += 2 * 3.14159f;
			}
		}

		*u /= 2 * 3.14159f;
		*v = (-ny + 1) / 2;
	}
}