#include <algorithm>
#include <cassert>

#include "mpp/Config.h"
#include "mpp/ProgrammaticModelStream.h"

using namespace std;

namespace mpp
{
	ProgrammaticModelStream::ProgrammaticModelStream()
		: ModelStream()
	{
	}

	void ProgrammaticModelStream::createMeshDataStreams()
	{
		// Set component streams for each mesh
		for (MeshDataStreamDefinition& meshDef: mMeshDataDefinitions)
		{
			int vertexOffset = 0;
			int vertexStride = 0;

			for (int i = 0; i < meshDef.specification.getNumVertexBufferAttributeLayouts(); ++i)
			{
				auto layout = meshDef.specification.getVertexBufferAttributeLayout(i);

				for (int j = 0; j < layout.getNumAttributes(); ++j)
				{
					auto attrib = layout.getAttribute(j);
					vertexStride += mesh::Vertex::getComponentSize(attrib.component);
				}
			}

			// Set counts
			meshDef.vertexCount = (int)meshDef.vertexData.size() / vertexStride;

			int elementSize = mesh::Primitive::size(meshDef.specification.getPrimitiveType());
			int indexWidthBytes = meshDef.indexWidth / 8;

			meshDef.primitiveCount = meshDef.specification.verticesIndexed() ? (meshDef.indexData.size() / (elementSize * indexWidthBytes)) : (meshDef.vertexCount / elementSize);

			// Go through each component in order, and build streams.
			int srcVertexDataSize = meshDef.vertexData.size() * sizeof(float);

			float* dataPtr = new float[meshDef.vertexData.size()];
			memcpy(dataPtr, &(meshDef.vertexData[0]), srcVertexDataSize);

			auto sharedDataPtr = std::shared_ptr<const int8>((const int8*)dataPtr, [](const int8 *p) { delete[] p; });

			for (int i = 0; i < meshDef.specification.getNumVertexBufferAttributeLayouts(); ++i)
			{
				auto layout = meshDef.specification.getVertexBufferAttributeLayout(i);

				for (int j = 0; j < layout.getNumAttributes(); ++j)
				{
					auto attrib = layout.getAttribute(j);

					VertexDataStreamDefinition vertexStreamDef;

					vertexStreamDef.data = sharedDataPtr;
					vertexStreamDef.dataType = mpp::mesh::Vertex::DataType::Float;
					vertexStreamDef.offset = vertexOffset;
					vertexStreamDef.stride = vertexStride;

					switch (attrib.component)
					{
					case mesh::Vertex::Component::Position2:
						meshDef.componentStreams[mpp::mesh::Vertex::Component::Position2] = vertexStreamDef;
						vertexOffset += 2;
						break;

					case mesh::Vertex::Component::Position3:
						meshDef.componentStreams[mpp::mesh::Vertex::Component::Position2] = vertexStreamDef;
						meshDef.componentStreams[mpp::mesh::Vertex::Component::Position3] = vertexStreamDef;
						vertexOffset += 3;
						break;

					case mesh::Vertex::Component::Position4:
						meshDef.componentStreams[mpp::mesh::Vertex::Component::Position2] = vertexStreamDef;
						meshDef.componentStreams[mpp::mesh::Vertex::Component::Position3] = vertexStreamDef;
						meshDef.componentStreams[mpp::mesh::Vertex::Component::Position4] = vertexStreamDef;
						vertexOffset += 4;
						break;

					case mesh::Vertex::Component::TexCoord2:
						meshDef.componentStreams[mpp::mesh::Vertex::Component::TexCoord2] = vertexStreamDef;
						vertexOffset += 2;
						break;

					case mesh::Vertex::Component::TexCoord3:
						meshDef.componentStreams[mpp::mesh::Vertex::Component::TexCoord2] = vertexStreamDef;
						meshDef.componentStreams[mpp::mesh::Vertex::Component::TexCoord3] = vertexStreamDef;
						vertexOffset += 3;
						break;

					case mesh::Vertex::Component::TexCoord4:
						meshDef.componentStreams[mpp::mesh::Vertex::Component::TexCoord2] = vertexStreamDef;
						meshDef.componentStreams[mpp::mesh::Vertex::Component::TexCoord3] = vertexStreamDef;
						meshDef.componentStreams[mpp::mesh::Vertex::Component::TexCoord4] = vertexStreamDef;
						vertexOffset += 4;
						break;

					case mesh::Vertex::Component::Colour1:
						meshDef.componentStreams[mpp::mesh::Vertex::Component::Colour1] = vertexStreamDef;
						vertexOffset += 1;
						break;

					case mesh::Vertex::Component::Colour3:
						meshDef.componentStreams[mpp::mesh::Vertex::Component::Colour1] = vertexStreamDef;
						meshDef.componentStreams[mpp::mesh::Vertex::Component::Colour3] = vertexStreamDef;
						vertexOffset += 3;
						break;

					case mesh::Vertex::Component::Colour4:
						meshDef.componentStreams[mpp::mesh::Vertex::Component::Colour3] = vertexStreamDef;
						meshDef.componentStreams[mpp::mesh::Vertex::Component::Colour4] = vertexStreamDef;
						vertexOffset += 4;
						break;

					case mesh::Vertex::Component::Normal3:
						meshDef.componentStreams[mpp::mesh::Vertex::Component::Normal3] = vertexStreamDef;
						vertexOffset += 3;
						break;
					}
				}
			}
		}
	}

	int ProgrammaticModelStream::getNumMeshes() const
	{
		return (int)mMeshDataDefinitions.size();
	}

	mesh::MeshSpecification const& ProgrammaticModelStream::getMeshSpecification(int meshIndex) const
	{
		return mMeshDataDefinitions[meshIndex].specification;
	}

	void ProgrammaticModelStream::getMeshCounts(int meshIndex, int* primitiveCount, int* vertexCount)
	{
		auto meshDef = mMeshDataDefinitions[meshIndex];

		*primitiveCount = meshDef.primitiveCount;
		*vertexCount = meshDef.vertexCount;
	}

	ModelStream::VertexDataStreamDefinition ProgrammaticModelStream::getMeshDataStream(int meshIndex, mesh::Vertex::Component component) const
	{
		return mMeshDataDefinitions[meshIndex].componentStreams.at(component);
	}

	int ProgrammaticModelStream::getMeshIndexWidth(int meshIndex) const
	{
		return mMeshDataDefinitions[meshIndex].indexWidth;
	}

	float ProgrammaticModelStream::getMeshPointSize(int meshIndex) const
	{
		return mMeshDataDefinitions[meshIndex].pointSize;
	}

	uint8 const* ProgrammaticModelStream::getMeshIndexData(int meshIndex) const
	{
		return &(mMeshDataDefinitions[meshIndex].indexData[0]);
	}

	string const& ProgrammaticModelStream::getMeshName(int meshIndex) const
	{
		return mMeshDataDefinitions[meshIndex].name;
	}

	string const& ProgrammaticModelStream::getMeshMaterial(int meshIndex) const
	{
		return mMeshDataDefinitions[meshIndex].material;
	}

	int ProgrammaticModelStream::createMesh(string const& name, mesh::MeshSpecification const& specification, string const& material, int indexWidth, float pointSize)
	{
		if (getMeshId(name) >= 0)
		{
			string errMsg = "Mesh '" + name + "' already defined in programmatic model stream.";
			throw exception(errMsg.c_str());
		}

		if (indexWidth != 16 && indexWidth != 32)
		{
			throw exception("Index width must be either 16 or 32.");
		}

		int index = mMeshDataDefinitions.size();
		
		MeshDataStreamDefinition meshDef;
		meshDef.specification = specification;
		meshDef.name = name;
		meshDef.indexWidth = indexWidth;
		meshDef.pointSize = pointSize;
		meshDef.material = material;
		
		mMeshDataDefinitions.push_back(meshDef);

		return index;
	}

	int ProgrammaticModelStream::getMeshId(string const& name) const
	{
		for (int i = 0; i < (int)mMeshDataDefinitions.size(); ++i)
		{
			if (mMeshDataDefinitions[i].name == name)
			{
				return i;
			}
		}
		return -1;
	}	

	void ProgrammaticModelStream::addPoint(int meshIndex, uint32 v)
	{
		MeshDataStreamDefinition& meshDef = mMeshDataDefinitions[meshIndex];

		if (meshDef.indexWidth == 16)
		{
			meshDef.indexData.push_back(v & 255);
			meshDef.indexData.push_back((v >> 8) & 255);
		}
		else
		{
			meshDef.indexData.push_back(v & 255);
			meshDef.indexData.push_back((v >> 8) & 255);
			meshDef.indexData.push_back((v >> 16) & 255);
			meshDef.indexData.push_back(v >> 24);
		}
	}

	void ProgrammaticModelStream::addLine(int meshIndex, uint32 v0, uint32 v1)
	{
		MeshDataStreamDefinition& meshDef = mMeshDataDefinitions[meshIndex];


		if (meshDef.indexWidth == 16)
		{
			meshDef.indexData.push_back(v0 & 255);
			meshDef.indexData.push_back((v0 >> 8) & 255);
			meshDef.indexData.push_back(v1 & 255);
			meshDef.indexData.push_back((v1 >> 8) & 255);
		}
		else
		{
			meshDef.indexData.push_back(v0 & 255);
			meshDef.indexData.push_back((v0 >> 8) & 255);
			meshDef.indexData.push_back((v0 >> 16) & 255);
			meshDef.indexData.push_back(v0 >> 24);
			meshDef.indexData.push_back(v1 & 255);
			meshDef.indexData.push_back((v1 >> 8) & 255);
			meshDef.indexData.push_back((v1 >> 16) & 255);
			meshDef.indexData.push_back(v1 >> 24);
		}
	}

	void ProgrammaticModelStream::addTriangle(int meshIndex, uint32 v0, uint32 v1, uint32 v2)
	{
		MeshDataStreamDefinition& meshDef = mMeshDataDefinitions[meshIndex];
		
		if (meshDef.indexWidth == 16)
		{
			meshDef.indexData.push_back(v0 & 255);
			meshDef.indexData.push_back((v0 >> 8) & 255);
			meshDef.indexData.push_back(v1 & 255);
			meshDef.indexData.push_back((v1 >> 8) & 255);
			meshDef.indexData.push_back(v2 & 255);
			meshDef.indexData.push_back((v2 >> 8) & 255);
		}
		else
		{
			meshDef.indexData.push_back(v0 & 255);
			meshDef.indexData.push_back((v0 >> 8) & 255);
			meshDef.indexData.push_back((v0 >> 16) & 255);
			meshDef.indexData.push_back(v0 >> 24);
			meshDef.indexData.push_back(v1 & 255);
			meshDef.indexData.push_back((v1 >> 8) & 255);
			meshDef.indexData.push_back((v1 >> 16) & 255);
			meshDef.indexData.push_back(v1 >> 24);
			meshDef.indexData.push_back(v2 & 255);
			meshDef.indexData.push_back((v2 >> 8) & 255);
			meshDef.indexData.push_back((v2 >> 16) & 255);
			meshDef.indexData.push_back(v2 >> 24);
		}
	}
	
}