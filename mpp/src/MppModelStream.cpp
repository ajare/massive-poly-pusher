#include "mpp/Config.h"
#include "mpp/MppModelStream.h"
#include "mpp/mesh/ModelSerializer.h"

#define FLAG_INDEXED_VERTICES 0x0001

using namespace std;

namespace mpp
{
	MppModelStream::MppModelStream(string const& filename)
		: mFilename(filename)
	{
		mesh::ModelSerializer ser;

		// Create child ResourceStreams
		auto materialInfo = ser.peakMaterialInformation(filename);

		for (auto const& mi: materialInfo)
		{
			// Create program stream based on MeshSpec and shaders, or by
			// loading files
			auto const& shaders = mi.second.getShaders();
			for (auto const& shader: shaders)
			{
				if (shader.name != "")
				{
					// Create shader from file
				}
				else
				{
					// Create default 2d/3d shader
				}
			}

			// Create texture streams if required

			// Create material stream
		}
	}

	MppModelStream::~MppModelStream()
	{
		for (auto it : mMeshDataDefinitions)
		{
			delete it;
		}
	}

	void MppModelStream::createMeshDataStreams()
	{
		mesh::ModelSerializer ser;
		ser.load(mFilename);

		// Create meshes
		for (int i = 0; i < ser.getMeshCount(); ++i)
		{
			auto dataStreamDef = new MeshDataStreamDefinition();
			mMeshDataDefinitions.push_back(dataStreamDef);

			dataStreamDef->specification = ser.getMeshSpecification(i);

			dataStreamDef->name = ser.getName(i);

			dataStreamDef->material = ser.getMaterial(i);
			dataStreamDef->indexWidth = ser.getIndexWidth(i);
			dataStreamDef->indexData = ser.getIndexData(i);

			dataStreamDef->primitiveType = ser.getPrimitiveType(i);
			dataStreamDef->primitiveCount = ser.getPrimitiveCount(i);

			for (int j = 0; j < dataStreamDef->specification.getNumVertexBufferAttributeLayouts(); ++j)
			{
				auto layout = dataStreamDef->specification.getVertexBufferAttributeLayout(j);

				int vertexCount, vertexStride;
				shared_ptr<const int8> vertexData;

				ser.getVertexStream(i, j, &vertexCount, &vertexStride, &vertexData);

				// All buffers will have the same vertex count.
				dataStreamDef->vertexCount = vertexCount;

				int offset = 0;
				for (int k = 0; k < layout.getNumAttributes(); ++k)
				{
					VertexDataStreamDefinition vertexStreamDef;

					vertexStreamDef.data = vertexData;

					auto attrib = layout.getAttribute(k);

					vertexStreamDef.dataType = attrib.dataType;
					vertexStreamDef.offset = offset;
					vertexStreamDef.stride = vertexStride;

					dataStreamDef->componentStreams[attrib.component] = vertexStreamDef;

					offset += attrib.sizeInBytes();
				}
			}
		}
	}

	ModelStream::VertexDataStreamDefinition MppModelStream::getMeshDataStream(int meshIndex, mesh::Vertex::Component component) const
	{
		return mMeshDataDefinitions[meshIndex]->componentStreams.at(component);
	}

	int MppModelStream::getMeshIndexWidth(int meshIndex) const
	{
		return mMeshDataDefinitions[meshIndex]->indexWidth;
	}

	float MppModelStream::getMeshPointSize(int meshIndex) const
	{
		return mMeshDataDefinitions[meshIndex]->pointSize;
	}

	uint8 const* MppModelStream::getMeshIndexData(int meshIndex) const
	{
		return mMeshDataDefinitions[meshIndex]->indexData.get();
	}

	string const& MppModelStream::getMeshName(int meshIndex) const
	{
		return mMeshDataDefinitions[meshIndex]->name;
	}

	string const& MppModelStream::getMeshMaterial(int meshIndex) const
	{
		return mMeshDataDefinitions[meshIndex]->material;
	}

	int MppModelStream::getNumMeshes() const
	{
		return (int)mMeshDataDefinitions.size();
	}

	void MppModelStream::getMeshCounts(int meshIndex, int* primitiveCount, int* vertexCount)
	{
		*primitiveCount = mMeshDataDefinitions[meshIndex]->primitiveCount;
		*vertexCount = mMeshDataDefinitions[meshIndex]->vertexCount;
	}

	mesh::MeshSpecification const& MppModelStream::getMeshSpecification(int meshIndex) const
	{
		return mMeshDataDefinitions[meshIndex]->specification;
	}

}