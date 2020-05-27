#include <algorithm>
#include <cassert>

#include "mpp/Config.h"
#include "mpp/PrimitiveModelStream.h"

using namespace std;

namespace mpp
{
	PrimitiveModelStream::PrimitiveModelStream(mesh::MeshSpecification const& meshSpec, string const& material)
		: ModelStream()
	{
		mMeshDataDefinition.specification = meshSpec;

		mMeshDataDefinition.name = "0";
		mMeshDataDefinition.material = material;
		mMeshDataDefinition.indexWidth = 32;
	}

	void PrimitiveModelStream::createMeshDataStreams()
	{
		int vertexOffset = 0;
		int vertexStride = 0;

		for (int i = 0; i < mMeshDataDefinition.specification.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto layout = mMeshDataDefinition.specification.getVertexBufferAttributeLayout(i);

			for (int j = 0; j < layout.getNumAttributes(); ++j)
			{
				auto attrib = layout.getAttribute(j);
				vertexStride += attrib.sizeInBytes();
			}
		}

		// Set counts
		mMeshDataDefinition.vertexCount = (int)mMeshDataDefinition.vertexData.size() / vertexStride;

		int elementSize = mesh::Primitive::size(mMeshDataDefinition.specification.getPrimitiveType());
		int indexWidthBytes = mMeshDataDefinition.indexWidth / 8;

		mMeshDataDefinition.primitiveCount = mMeshDataDefinition.specification.verticesIndexed() ? ((mMeshDataDefinition.indexData.size() * sizeof(uint32)) / (elementSize * indexWidthBytes)) : (mMeshDataDefinition.vertexCount / elementSize);

		// Go through each component in order, and build streams.
		int srcVertexDataSize = mMeshDataDefinition.vertexData.size() * sizeof(float);

		float* dataPtr = new float[mMeshDataDefinition.vertexData.size()];
		memcpy(dataPtr, &(mMeshDataDefinition.vertexData[0]), srcVertexDataSize);

		auto sharedDataPtr = std::shared_ptr<const int8>((const int8*)dataPtr, [](const int8 *p) { delete[] p; });

		for (int i = 0; i < mMeshDataDefinition.specification.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto layout = mMeshDataDefinition.specification.getVertexBufferAttributeLayout(i);

			for (int j = 0; j < layout.getNumAttributes(); ++j)
			{
				auto attrib = layout.getAttribute(j);

				VertexDataStreamDefinition vertexStreamDef;

				vertexStreamDef.data = sharedDataPtr;
				vertexStreamDef.dataType = attrib.dataType;
				vertexStreamDef.offset = vertexOffset;
				vertexStreamDef.stride = vertexStride;

				vertexOffset += attrib.sizeInBytes();

				switch (attrib.component)
				{
				case mesh::Vertex::Component::Position2:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Position2] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::Position3:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Position2] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Position3] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::Position4:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Position2] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Position3] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Position4] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::TexCoord2:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::TexCoord2] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::TexCoord3:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::TexCoord2] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::TexCoord3] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::TexCoord4:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::TexCoord2] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::TexCoord3] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::TexCoord4] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::Colour1:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Colour1] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::Colour3:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Colour1] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Colour3] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::Colour4:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Colour3] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Colour4] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::Normal3:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Normal3] = vertexStreamDef;
					break;

				case mesh::Vertex::Component::Normal4:
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Normal3] = vertexStreamDef;
					mMeshDataDefinition.componentStreams[mpp::mesh::Vertex::Component::Normal4] = vertexStreamDef;
					break;
				}
			}
		}
	}

	int PrimitiveModelStream::getNumMeshes() const
	{
		return 1;
	}

	mesh::MeshSpecification const& PrimitiveModelStream::getMeshSpecification(int meshIndex) const
	{
		return mMeshDataDefinition.specification;
	}

	void PrimitiveModelStream::getMeshCounts(int meshIndex, int* primitiveCount, int* vertexCount)
	{
		*primitiveCount = mMeshDataDefinition.primitiveCount;
		*vertexCount = mMeshDataDefinition.vertexCount;
	}

	ModelStream::VertexDataStreamDefinition PrimitiveModelStream::getMeshDataStream(int meshIndex, mesh::Vertex::Component component) const
	{
		return mMeshDataDefinition.componentStreams.at(component);
	}

	int PrimitiveModelStream::getMeshIndexWidth(int meshIndex) const
	{
		return mMeshDataDefinition.indexWidth;
	}

	float PrimitiveModelStream::getMeshPointSize(int meshIndex) const
	{
		return mMeshDataDefinition.pointSize;
	}

	uint8 const* PrimitiveModelStream::getMeshIndexData(int meshIndex) const
	{
		return (uint8 const*)&(mMeshDataDefinition.indexData[0]);
	}

	string const& PrimitiveModelStream::getMeshName(int meshIndex) const
	{
		return mMeshDataDefinition.name;
	}

	string const& PrimitiveModelStream::getMeshMaterial(int meshIndex) const
	{
		return mMeshDataDefinition.material;
	}

	void PrimitiveModelStream::addTriangle(uint32 v0, uint32 v1, uint32 v2)
	{
		mMeshDataDefinition.indexData.push_back(v0);
		mMeshDataDefinition.indexData.push_back(v1);
		mMeshDataDefinition.indexData.push_back(v2);
		/*
		if (mMeshDataDefinition.indexWidth == 16)
		{
			mMeshDataDefinition.indexData.push_back(v0 & 255);
			mMeshDataDefinition.indexData.push_back((v0 >> 8) & 255);
			mMeshDataDefinition.indexData.push_back(v1 & 255);
			mMeshDataDefinition.indexData.push_back((v1 >> 8) & 255);
			mMeshDataDefinition.indexData.push_back(v2 & 255);
			mMeshDataDefinition.indexData.push_back((v2 >> 8) & 255);
		}
		else
		{
			mMeshDataDefinition.indexData.push_back(v0 & 255);
			mMeshDataDefinition.indexData.push_back((v0 >> 8) & 255);
			mMeshDataDefinition.indexData.push_back((v0 >> 16) & 255);
			mMeshDataDefinition.indexData.push_back(v0 >> 24);
			mMeshDataDefinition.indexData.push_back(v1 & 255);
			mMeshDataDefinition.indexData.push_back((v1 >> 8) & 255);
			mMeshDataDefinition.indexData.push_back((v1 >> 16) & 255);
			mMeshDataDefinition.indexData.push_back(v1 >> 24);
			mMeshDataDefinition.indexData.push_back(v2 & 255);
			mMeshDataDefinition.indexData.push_back((v2 >> 8) & 255);
			mMeshDataDefinition.indexData.push_back((v2 >> 16) & 255);
			mMeshDataDefinition.indexData.push_back(v2 >> 24);
		}
		*/
	}

}