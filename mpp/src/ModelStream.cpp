#include <iterator>
#include <algorithm>
#include <cstdarg>

#include "mpp/ModelStream.h"
#include "mpp/RenderSystem.h"
#include "mpp/VertexBuffer.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{
	using namespace mesh;

	/*
	 * Constructor.
	 *
	 */
	ModelStream::ModelStream()
	{
	}

	/*
	 * Destructor.
	 *
	 */
	ModelStream::~ModelStream()
	{
		for (auto it: mMeshDefinitions)
		{
			delete it;
		}
	}

	/*
	 * Get resource stream type.
	 *
	 */
	string ModelStream::getType()
	{
		return "Model";
	}

	/*
	 * Are the streams tightly packed?
	 *
	 */
	bool ModelStream::streamsAreTightlyPacked(VertexBufferAttributeLayout const& bufferSpec, map<Vertex::Component, VertexDataStreamDefinition> componentStreams)
	{
		int8 const* streamData1 = (componentStreams.begin())->second.data.get();
		int streamStride1 = (componentStreams.begin())->second.stride;

		for (auto it = componentStreams.begin(); it != componentStreams.end(); ++it)
		{
			// Are the sources the same?
			if (it->second.data.get() != streamData1)
			{
				return false;
			}

			// Are the strides all the same?
			if (it->second.stride != streamStride1)
			{
				return false;
			}
		}

		// Do our channels have the same data type as the stream, and do they map
		// in the same order?
		int vertexStride = 0;
		int vertexOffset = 0;
		for (int i = 0; i < bufferSpec.getNumAttributes(); ++i)
		{
			auto const& attrib = bufferSpec.getAttribute(i);
			auto const& stream = componentStreams[attrib.component];
			if (stream.dataType != attrib.dataType)
			{
				return false;
			}

			if (stream.offset != vertexOffset)
			{
				return false;
			}

			vertexStride += Vertex::getComponentSize(attrib.component) * Vertex::getDataTypeSize(attrib.dataType) + attrib.paddingBytes;
			vertexOffset += Vertex::getComponentSize(attrib.component) + attrib.paddingBytes;
		}

		if (vertexStride != streamStride1)
		{
			return false;
		}

		return true;
	}

	/*
	 * Create a vertex buffer definition by copying over the data in one go, as we know
	 * that the source is set up to match our target layout.
	 *
	 */
	int8* ModelStream::copyVertexBufferData(VertexBufferAttributeLayout const& bufferSpec, VertexDataStreamDefinition componentStream, int vertexCount, int vertexStride)
	{
		int8* bufData = new int8[vertexStride * vertexCount];

		memcpy(bufData, componentStream.data.get(), vertexCount * vertexStride);
		return bufData;
	}

	/*
	 * Create a vertex buffer definition by copying each attribute from an interlaced source.
	 *
	 */
	int8* ModelStream::deinterlaceVertexBufferData(VertexBufferAttributeLayout const& bufferSpec, map<Vertex::Component, VertexDataStreamDefinition> const& componentStreams, int vertexCount, int vertexStride)
	{
		int8* bufData = new int8[vertexStride * vertexCount];
		int8* bufDataPtr = bufData;

		for (int i = 0; i < vertexCount; ++i)
		{
			for (int j = 0; j < bufferSpec.getNumAttributes(); ++j)
			{
				auto const& attrib = bufferSpec.getAttribute(j);
				auto const& stream = componentStreams.at(attrib.component);

				int vertexOffset = (stream.stride * i + stream.offset);
				int vertexWidth = Vertex::getComponentSize(attrib.component);

				// Convert to desired datatype.
				switch (attrib.dataType)
				{
				case Vertex::DataType::Byte:
					for (int k = 0; k < vertexWidth; ++k)
					{
						*bufDataPtr++ = ((int8 const*)stream.data.get())[vertexOffset + k];
					}
					break;

				case Vertex::DataType::UnsignedByte:
					for (int k = 0; k < vertexWidth; ++k)
					{
						*bufDataPtr++ = ((uint8 const*)stream.data.get())[vertexOffset + k];
					}
					break;

				case Vertex::DataType::Float:
					memcpy(bufDataPtr, stream.data.get() + vertexOffset * Vertex::getDataTypeSize(attrib.dataType), vertexWidth * Vertex::getDataTypeSize(attrib.dataType));
					bufDataPtr += vertexWidth * Vertex::getDataTypeSize(attrib.dataType);
					break;

				default:
					THROW_MPP("Cannot convert data to unsupported type.", __LINE__, __FILE__, __FUNCTION__);
				}

				for (int k = 0; k < attrib.paddingBytes; ++k)
				{
					*bufDataPtr++ = 0;
				}
			}
		}

		return bufData;
	}

	/*
	 * Load in data.
	 *
	 */
	void ModelStream::loadImpl()
	{
		createMeshDataStreams();

		// Create MeshDefinitions from data streams
		for (int i = 0; i < getNumMeshes(); ++i)
		{
			int primitiveCount, vertexCount;
			getMeshCounts(i, &primitiveCount, &vertexCount);

			mesh::MeshSpecification const& meshSpec = getMeshSpecification(i);
			string material = getMeshMaterial(i);

			auto primitiveType = meshSpec.getPrimitiveType();
			MeshDefinition* meshDef = createMeshDefinition(getMeshName(i), primitiveType, primitiveCount, meshSpec.getStorageType(), material, getMeshIndexWidth(i), getMeshPointSize(i));

			// Set index data if we have any.
			if (meshSpec.verticesIndexed())
			{
				uint8 const* indexData = getMeshIndexData(i);
				int indexWidth = getMeshIndexWidth(i);
				int indexWidthBytes = indexWidth / 8;

				if (indexData)
				{
					size_t dataSize = primitiveCount * mesh::Primitive::size(primitiveType) * indexWidthBytes;

					uint8* indexBuffer = new uint8[dataSize];
					memcpy(indexBuffer, indexData, dataSize);

					meshDef->setIndexData(shared_ptr<const uint8>(indexBuffer, [](uint8*p) { delete[] p; }));
				}
			}

			
			for (int j = 0; j < meshSpec.getNumVertexBufferAttributeLayouts(); ++j)
			{
				auto const& bufferSpec = meshSpec.getVertexBufferAttributeLayout(j);

				// Acquire the vertex streams needed and work out the stride.
				int vertexStride = 0;
				map<Vertex::Component, VertexDataStreamDefinition> componentStreams;
				for (int k = 0; k < bufferSpec.getNumAttributes(); ++k)
				{
					auto const& attrib = bufferSpec.getAttribute(k);

					componentStreams[attrib.component] = getMeshDataStream(i, attrib.component);
					vertexStride += Vertex::getComponentSize(attrib.component) * Vertex::getDataTypeSize(attrib.dataType) + attrib.paddingBytes;
				}

				// See whether or not the streams are the same, and are packed tightly.
				// If they are, we can load the data in one go.
				int8* bufData = streamsAreTightlyPacked(bufferSpec, componentStreams)
					? copyVertexBufferData(bufferSpec, componentStreams.begin()->second, vertexCount, vertexStride)
					: deinterlaceVertexBufferData(bufferSpec, componentStreams, vertexCount, vertexStride);

				VertexBufferDefinition* vertexBufDef = meshDef->createVertexBufferDefinition(bufferSpec, vertexCount, vertexStride, shared_ptr<const int8>(bufData, [](int8 const* p) { delete[] p; }));
			}
		}
	}

	/*
	 * Create a new mesh and return it.
	 *
	 */
	MeshDefinition* ModelStream::createMeshDefinition(string const& name, mesh::Primitive::Type type, int primitiveCount, mesh::VertexBufferStorageType storageType, string const& material, int indexWidth, float pointSize)
	{
		MeshDefinition* md = new MeshDefinition(material, type, storageType, primitiveCount, indexWidth, pointSize);
		md->setName(name);

		mMeshDefinitions.push_back(md);

		return md;
	}

	/*
	 * Get number of meshes in this stream.
	 *
	 */
	int ModelStream::getNumMeshDefinitions() const
	{
		return (int)mMeshDefinitions.size();
	}
	
	/*
	 * Get indexed mesh definition.
	 *
	 */
	MeshDefinition* ModelStream::getMeshDefinition(int index)
	{
		assert((index >= 0 && index < getNumMeshDefinitions()) && "ModelStream::getMeshDefinition() 'index' argument out of range!");
		return mMeshDefinitions[index];
	}
}