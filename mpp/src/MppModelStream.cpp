#include "utils/FileSystem.h"

#include "mpp/Config.h"
#include "mpp/MppModelStream.h"
#include "mpp/ProgrammaticMaterialStream.h"
#include "mpp/ProgrammaticTextureStream.h"
#include "mpp/ProgrammaticStringStream.h"
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
		createQualitySetting("");
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

			auto mStr = new ProgrammaticMaterialStream(resMgr);
			
			mStr->setProgram2d(matInfo.getPositionType() == mesh::MaterialInformation::PositionType::p2D);
			mStr->setMeshSpecification(meshSpec);

			mStr->setProgramVertexShaderResource(vertexShader);
			mStr->setProgramFragmentShaderResource(vertexShader);

			// Add program resources if required
			if (vertexShaderIsFile)
			{
				// Add a File StringStream child to MaterialStream
				string vertexShaderFilename = utils::FileSystem::concatPaths(
					utils::FileSystem::baseDirectory(mFilename),
					vertexShader);

				auto strStr = new ProgrammaticStringStream(resMgr);
				strStr->setFile(vertexShaderFilename);
				mStr->addChild(vertexShader, ResourceStreamPtr(strStr));
			}
			if (fragmentShaderIsFile)
			{
				// Add a File StringStream child to MaterialStream
				string fragmentShaderFilename = utils::FileSystem::concatPaths(
					utils::FileSystem::baseDirectory(mFilename),
					fragmentShader);

				auto strStr = new ProgrammaticStringStream(resMgr);
				strStr->setFile(fragmentShaderFilename);
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

					auto texStr = new ProgrammaticTextureStream(resMgr);
					texStr->setFile(TextureStream::Target::Texture2D, textureFilename, resMgr->getImageLoadFunction());
					texStr->setFiltering(TextureParams::MinFilter::Linear, TextureParams::MagFilter::Linear);

					mStr->addChild(texture.resource, ResourceStreamPtr(texStr));
					mStr->setTextureChild(texture.binding, texture.resource);
				}
			}

			// Add uniforms
			auto const& uniforms = matInfo.getUniforms();
			for (auto const& uniform: uniforms)
			{
				if (uniform.type == "int")
				{
					int32_t values[4];
					for (size_t i = 0; i < uniform.numComponents; ++i)
					{
						values[i] = any_cast<int32_t>(uniform.values[0]);
					}

					mStr->setUniform(uniform.name, uniform.numComponents, values);
				}
				else if (uniform.type == "uint")
				{
					uint32_t values[4];
					for (size_t i = 0; i < uniform.numComponents; ++i)
					{
						values[i] = any_cast<uint32_t>(uniform.values[0]);
					}

					mStr->setUniform(uniform.name, uniform.numComponents, values);
				}
				else if (uniform.type == "float")
				{
					float values[4];
					for (size_t i = 0; i < uniform.numComponents; ++i)
					{
						values[i] = any_cast<float>(uniform.values[0]);
					}

					mStr->setUniform(uniform.name, uniform.numComponents, values);
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
		for (size_t i = 0; i < ser.getMeshCount(); ++i)
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

				size_t vertexCount, vertexStride;
				shared_ptr<const int8_t> vertexData;

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

	ModelStream::VertexDataStreamDefinition MppModelStream::getMeshDataStream(size_t meshIndex, mesh::Vertex::Component component) const
	{
		return mMeshDataDefinitions[meshIndex]->componentStreams.at(component);
	}

	size_t MppModelStream::getMeshIndexWidth(size_t meshIndex) const
	{
		return mMeshDataDefinitions[meshIndex]->indexWidth;
	}

	float MppModelStream::getMeshPointSize(size_t meshIndex) const
	{
		return mMeshDataDefinitions[meshIndex]->pointSize;
	}

	uint8_t const* MppModelStream::getMeshIndexData(size_t meshIndex) const
	{
		return mMeshDataDefinitions[meshIndex]->indexData.get();
	}

	string const& MppModelStream::getMeshName(size_t meshIndex) const
	{
		return mMeshDataDefinitions[meshIndex]->name;
	}

	string const& MppModelStream::getMeshMaterial(size_t meshIndex) const
	{
		return mMeshDataDefinitions[meshIndex]->material;
	}

	size_t MppModelStream::getNumMeshes() const
	{
		return mMeshDataDefinitions.size();
	}

	void MppModelStream::getMeshCounts(size_t meshIndex, size_t* primitiveCount, size_t* vertexCount)
	{
		*primitiveCount = mMeshDataDefinitions[meshIndex]->primitiveCount;
		*vertexCount = mMeshDataDefinitions[meshIndex]->vertexCount;
	}

	mesh::MeshSpecification const& MppModelStream::getMeshSpecification(size_t meshIndex) const
	{
		return mMeshDataDefinitions[meshIndex]->specification;
	}

	string MppModelStream::markUpMaterialName(string const& name, string const& material)
	{
		return name + "/" + material;
	}
}