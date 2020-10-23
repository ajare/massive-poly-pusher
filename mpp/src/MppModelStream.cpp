#include "utils/FileSystem.h"

#include "mpp/Config.h"
#include "mpp/MppModelStream.h"
#include "mpp/ProgrammaticMaterialStream.h"
#include "mpp/TextureStream.h"
#include "mpp/FileStringStream.h"
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
		auto reader = ser.getReader(mFilename);

		// Create child ResourceStreams
		string vertexShader{ "" }, fragmentShader{ "" };

		// Create a material resource for each unique combination of mesh (spec) and
		// the material it uses
		auto numMeshes = reader.getNumMeshes();
		for (size_t i = 0; i < numMeshes; ++i)
		{
			auto const& matInfo = reader.getMaterialByMeshId(i);
			auto const& meshSpec = reader.getMeshSpecificationByMeshId(i);

			// Create program stream based on MeshSpec and shaders, or by
			// loading files
			bool vertexShaderIsFile, fragmentShaderIsFile;
			bool foundVertexShader{ false }, foundFragmentShader{ false };
			auto const& shaders = matInfo.getShaders();
			for (auto const& shader: shaders)
			{
				switch (shader.type)
				{
				case mesh::MaterialInformation::Shader::Type::Vertex:
					foundVertexShader = true;
					vertexShaderIsFile = shader.name != "";
					vertexShader = shader.name;
					break;

				case mesh::MaterialInformation::Shader::Type::Fragment:
					foundFragmentShader = true;
					fragmentShaderIsFile = shader.name != "";
					fragmentShader = shader.name;
					break;
				}
			}

			if (!foundVertexShader)
			{
				THROW_MPP("No vertex shader specified for program in material '" + matInfo.getName() + "'",
					__LINE__, __FILE__, __func__);
			}
			if (!foundFragmentShader)
			{
				THROW_MPP("No fragment shader specified for program in material '" + matInfo.getName() + "'",
					__LINE__, __FILE__, __func__);
			}

			bool is2d = matInfo.getPositionType() == mesh::MaterialInformation::PositionType::p2D;
			auto mStr = new ProgrammaticMaterialStream(
				resMgr,
				is2d, 
				meshSpec, 
				vertexShader, 
				vertexShaderIsFile,
				fragmentShader, 
				fragmentShaderIsFile);

			// Add program resources if required
			if (vertexShaderIsFile)
			{
				// Add a File StringStream child to MaterialStream
				string vertexShaderFilename = utils::FileSystem::concatPaths(
					utils::FileSystem::baseDirectory(mFilename),
					vertexShader);

				auto strStr = new FileStringStream(resMgr, vertexShaderFilename);
				mStr->addChild(vertexShader, ResourceStreamPtr(strStr));
			}
			if (fragmentShaderIsFile)
			{
				// Add a File StringStream child to MaterialStream
				string fragmentShaderFilename = utils::FileSystem::concatPaths(
					utils::FileSystem::baseDirectory(mFilename),
					fragmentShader);

				auto strStr = new FileStringStream(resMgr, fragmentShaderFilename);
				mStr->addChild(fragmentShader, ResourceStreamPtr(strStr));
			}
			// Create texture streams if required
			auto const& textures = matInfo.getTextures();
			for (auto const& texture: textures)
			{
				if (texture.isResource)
				{
					mStr->setTexture(texture.binding, texture.resource);
				}
				else
				{
					// Add a File TextureStream child to MaterialStream
					string textureFilename = utils::FileSystem::concatPaths(
						utils::FileSystem::baseDirectory(mFilename),
						texture.resource);

					auto texStr = new TextureStream(
						resMgr,
						textureFilename,
						resMgr->getImageLoadFunction(),
						true);

					mStr->addChild(texture.resource, ResourceStreamPtr(texStr));
					mStr->setTextureChild(texture.binding, texture.resource);
				}
			}

			addChild(matInfo.getName(), ResourceStreamPtr(mStr));
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