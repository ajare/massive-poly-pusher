#include "mpp/Config.h"
#include "mpp/MppModelStream.h"
#include "mpp/ProgrammaticMaterialStream.h"
#include "mpp/TextureStream.h"
#include "mpp/mesh/ModelSerializer.h"
#include "mpp/ResourceManager.h"

#define FLAG_INDEXED_VERTICES 0x0001

using namespace std;

namespace mpp
{
	MppModelStream::MppModelStream(ResourceManager* resourceMgr, string const& filename)
		: ModelStream(resourceMgr)
		, mFilename(filename)
	{
	}

	MppModelStream::~MppModelStream()
	{
		for (auto it : mMeshDataDefinitions)
		{
			delete it;
		}
	}

	void MppModelStream::createChildResourceStreamsImpl()
	{
		auto resMgr = getResourceMgr();

		mesh::ModelSerializer ser;

		// Create child ResourceStreams
		auto meshNames = ser.peakMeshNames(mFilename);
		auto materialInfo = ser.peakMaterialInformation(mFilename);
		auto meshSpecs = ser.peakMeshSpecification(mFilename);
		
		string vertexShader{ "" }, fragmentShader{ "" };

		// Create a material resource for each unique combination of mesh (spec) and
		// the material it uses
		for (auto const& meshName: meshNames)
		{
			auto matInfo = materialInfo[] // Need mesh name -> material mapping

			// Create program stream based on MeshSpec and shaders, or by
			// loading files
			bool shadersAreFiles{ false };
			auto const& shaders = mi.second.getShaders();
			for (auto const& shader: shaders)
			{
				if (shader.name != "")
				{
					shadersAreFiles = true;
				}

				switch (shader.type)
				{
				case mesh::MaterialInformation::Shader::Type::Vertex:
					vertexShader = shader.name;
					break;

				case mesh::MaterialInformation::Shader::Type::Fragment:
					fragmentShader = shader.name;
					break;
				}
			}

			bool is2d = mi.second.getPositionType() == mesh::MaterialInformation::PositionType::p2D;
			auto const& meshSpec = meshSpecs[mi.first];
			auto mStr = new ProgrammaticMaterialStream(
				resMgr,
				is2d, 
				meshSpec, 
				vertexShader, 
				fragmentShader, 
				shadersAreFiles);

			// Create texture streams if required
			auto const& textures = mi.second.getTextures();
			for (auto const& texture: textures)
			{
				if (!texture.isResource)
				{
					// Add a File TextureStream child to MaterialStream
					auto texStr = new TextureStream(
						resMgr,
						texture.resource,
						resMgr->getImageLoadFunction(),
						true);

					mStr->addChild(texture.resource, ResourceStreamPtr(texStr));
				}
				else
				{
					mStr->setTexture(texture.binding, texture.resource);
				}
			}

			addChild(mi.second.getName(), ResourceStreamPtr(mStr));
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

	string MppModelStream::markUpMaterialName(string const& name, string const& material)
	{
		return name + "/" + material;
	}
}