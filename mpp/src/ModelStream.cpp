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
	ModelStream::ModelStream(ResourceManager* resourceMgr)
		: ResourceStream(resourceMgr, "Model")
	{
		mQualitySettings.resize(1);
	}

	/*
	 * Destructor.
	 *
	 */
	ModelStream::~ModelStream()
	{
		for (auto& qs: mQualitySettings)
		{
			for (auto meshDef: qs.meshDefinitions)
			{
				delete meshDef;
			}
		}
	}

	/*
	 * Are the streams tightly packed?
	 *
	 */
	bool ModelStream::streamsAreTightlyPacked(VertexBufferAttributeLayout const& bufferSpec, map<Vertex::Component, VertexDataStreamDefinition> const& componentStreams)
	{
		auto streamData1 = (componentStreams.begin())->second.data.get();
		int streamStride1 = (componentStreams.begin())->second.stride;

		for (auto stream: componentStreams)
		{
			// Are the sources the same?
			if (stream.second.data.get() != streamData1)
			{
				return false;
			}

			// Are the strides all the same?
			if (stream.second.stride != streamStride1)
			{
				return false;
			}
		}

		// Do our channels have the same data type as the stream, and do they map
		// in the same order?
		int vertexOffset = 0;
		for (int i = 0; i < bufferSpec.getNumAttributes(); ++i)
		{
			auto const& attrib = bufferSpec.getAttribute(i);
			auto const& stream = componentStreams.at(attrib.component);
			if (stream.dataType != attrib.dataType)
			{
				return false;
			}

			if (stream.offset != vertexOffset)
			{
				return false;
			}

			vertexOffset += attrib.sizeInBytes();
		}

		if (vertexOffset != streamStride1)
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
	int8_t* ModelStream::copyVertexBufferData(VertexBufferAttributeLayout const& bufferSpec, VertexDataStreamDefinition componentStream, int vertexCount, int vertexStride)
	{
		int8_t* bufData = new int8_t[vertexStride * vertexCount];

		memcpy(bufData, componentStream.data.get(), vertexCount * vertexStride);
		return bufData;
	}

	/*
	 * Create a vertex buffer definition by copying each attribute from an interlaced source.
	 *
	 */
	int8_t* ModelStream::deinterlaceVertexBufferData(VertexBufferAttributeLayout const& bufferSpec, map<Vertex::Component, VertexDataStreamDefinition> const& componentStreams, int vertexCount, int vertexStride)
	{
		int8_t* bufData = new int8_t[vertexStride * vertexCount];
		int8_t* bufDataPtr = bufData;

		for (int i = 0; i < vertexCount; ++i)
		{
			for (int j = 0; j < bufferSpec.getNumAttributes(); ++j)
			{
				auto const& attrib = bufferSpec.getAttribute(j);
				auto const& stream = componentStreams.at(attrib.component);

				auto componentOffset = (stream.stride * i + stream.offset);
				auto componentCount = Vertex::getComponentSize(attrib.component);
				auto componentSize = attrib.sizeInBytes();

				// Convert to desired datatype.
				switch (attrib.dataType)
				{
				case Vertex::DataType::Byte:
					for (int k = 0; k < componentCount; ++k)
					{
						*bufDataPtr++ = ((int8_t const*)stream.data.get())[componentOffset + k];
					}
					break;

				case Vertex::DataType::UnsignedByte:
					for (int k = 0; k < componentCount; ++k)
					{
						*bufDataPtr++ = ((uint8_t const*)stream.data.get())[componentOffset + k];
					}
					break;

				case Vertex::DataType::Float:
				case Vertex::DataType::Int:
					memcpy(bufDataPtr, stream.data.get() + componentOffset, componentSize);
					bufDataPtr += componentSize;
					break;

				default:
					THROW_MPP("Cannot convert data to unsupported type.", __LINE__, __FILE__, __func__);
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
				uint8_t const* indexData = getMeshIndexData(i);
				int indexWidth = getMeshIndexWidth(i);
				int indexWidthBytes = indexWidth / 8;

				if (indexData)
				{
					size_t dataSize = primitiveCount * mesh::Primitive::size(primitiveType) * indexWidthBytes;

					uint8_t* indexBuffer = new uint8_t[dataSize];
					memcpy(indexBuffer, indexData, dataSize);

					meshDef->setIndexData(shared_ptr<const uint8_t>(indexBuffer, [](uint8_t*p) { delete[] p; }));
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
				int8_t* bufData = streamsAreTightlyPacked(bufferSpec, componentStreams)
					? copyVertexBufferData(bufferSpec, componentStreams.begin()->second, vertexCount, vertexStride)
					: deinterlaceVertexBufferData(bufferSpec, componentStreams, vertexCount, vertexStride);

				VertexBufferDefinition* vertexBufDef = meshDef->createVertexBufferDefinition(bufferSpec, vertexCount, vertexStride, shared_ptr<const int8_t>(bufData, [](int8_t const* p) { delete[] p; }));
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

		mQualitySettings[mQualitySetting].meshDefinitions.push_back(md);

		return md;
	}

	/*
	 * Get number of meshes in this stream.
	 *
	 */
	size_t ModelStream::getNumMeshDefinitions(uint32_t quality) const
	{
		return mQualitySettings[quality].meshDefinitions.size();
	}
	
	/*
	 * Get indexed mesh definition.
	 *
	 */
	MeshDefinition* ModelStream::getMeshDefinition(int index, uint32_t quality)
	{
		assert((index >= 0 && index < getNumMeshDefinitions()) && "ModelStream::getMeshDefinition() 'index' argument out of range!");
		return mQualitySettings[quality].meshDefinitions[index];
	}

	string ModelStream::markUpMaterialName(string const& name, string const& material)
	{
		return material;
	}

	uint32_t ModelStream::createQualitySetting(string const& name)
	{
		auto qualityId = mQualitySettings.size();
		mQualityNames[name] = qualityId;

		mQualitySettings.push_back(QualitySetting());
		return qualityId;
	}
}