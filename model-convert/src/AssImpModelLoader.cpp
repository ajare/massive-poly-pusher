#include <iostream>
#include <cassert>
#include <regex>

#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include <assimp/Importer.hpp>

#include <half/half.hpp>

#include "utils/StringUtils.h"
#include "utils/XmlReader.h"
#include "utils/XmlWriter.h"

#include "mpp/MppException.h"
#include "mpp/mesh/Vertex.h"

#include "AssImpModelLoader.h"

using namespace std;
using namespace Assimp;
using namespace mpp;
using namespace mpp::mesh;

/*
 * Constructor.
 *
 */
AssImpModelLoader::AssImpModelLoader(string const& filename, MeshSpecification const& meshSpec, map<string, mpp::mesh_specification_parser::ProgramInformation> const& programInfo, map<string, mpp::mesh_specification_parser::MaterialInformation> const& materialInfo, uint32 maxVerticesPerMesh, bool generateColours) :
	mFilename(filename),
	mSpecification(meshSpec),
	mProgramInfo(programInfo),
	mMaterialInfo(materialInfo),
	mMaxVerticesPerMesh(maxVerticesPerMesh),
	mGenerateColours(generateColours)
{
}

/*
 * Destructor.
 *
 */
AssImpModelLoader::~AssImpModelLoader()
{
	for (auto it : mMeshDataDefinitions)
	{
		delete it;
	}
}

/*
 * Add a vertex to list to be processed.
 *
 */
void AssImpModelLoader::addBuildVertex(aiMesh const* mesh, int index, aiMaterial* material, vector<float>& vertices, bool hasPositions, bool hasNormals, bool hasTexCoords, bool hasColours)
{
	// Positions
	if (hasPositions)
	{
		vertices.push_back(mesh->mVertices[index].x);
		vertices.push_back(mesh->mVertices[index].y);
		vertices.push_back(mesh->mVertices[index].z);
	}

	// Normals
	if (hasNormals)
	{
		vertices.push_back(mesh->mNormals[index].x);
		vertices.push_back(mesh->mNormals[index].y);
		vertices.push_back(mesh->mNormals[index].z);
	}

	// Texcoords
	if (hasTexCoords)
	{
		vertices.push_back(mesh->mTextureCoords[0][index].x);
		vertices.push_back(mesh->mTextureCoords[0][index].y);
	}

	// Colour
	if (hasColours)
	{
		vertices.push_back(mesh->mColors[0][index].r);
		vertices.push_back(mesh->mColors[0][index].g);
		vertices.push_back(mesh->mColors[0][index].b);
		vertices.push_back(mesh->mColors[0][index].a);
	}
	else if (mGenerateColours)
	{
		aiColor4D colour(1.0f, 1.0f, 1.0f, 1.0f);

		aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &colour);
		vertices.push_back(colour.r);
		vertices.push_back(colour.g);
		vertices.push_back(colour.b);
		vertices.push_back(1.0f);
	}
}

/*
 * Add a face to list to be processed.
 *
 */
void AssImpModelLoader::addBuildFace(uint32 index0, uint32 index1, uint32 index2, vector<uint32>& faces)
{
	faces.push_back(index0);
	faces.push_back(index1);
	faces.push_back(index2);
}

/*
 * Get the number of meshes in this model.
 *
 */
int AssImpModelLoader::getNumMeshes() const
{
	return (int)mMeshDataDefinitions.size();
}

/*
 * Get the triangle and vertex count for a specified mesh.
 *
 */
void AssImpModelLoader::getMeshCounts(int meshIndex, int* primitiveCount, int* vertexCount)
{
	*primitiveCount = mMeshDataDefinitions[meshIndex]->triangleCount;
	*vertexCount = mMeshDataDefinitions[meshIndex]->vertexCount;
}

/*
 * Get a pointer to a data stream for a mesh for particular component.
 *
 */
AssImpModelLoader::VertexDataStreamDefinition AssImpModelLoader::getMeshDataStream(int meshIndex, mpp::mesh::Vertex::Component component) const
{
	return mMeshDataDefinitions[meshIndex]->componentStreams.at(component);
}

/*
 * Get mesh index width;
 *
 */
int AssImpModelLoader::getMeshIndexWidth(int meshIndex) const
{
	return mMeshDataDefinitions[meshIndex]->indexWidth;
}

/*
 * Get mesh index data.
 *
 */
vector<uint8> const& AssImpModelLoader::getMeshIndexData(int meshIndex) const
{
	return mMeshDataDefinitions[meshIndex]->indexData;
}

/*
 * Get mesh name
 *
 */
string const& AssImpModelLoader::getMeshName(int meshIndex) const
{
	return mMeshDataDefinitions[meshIndex]->name;
}

/*
 * Get mesh material name.
 *
 */
string const& AssImpModelLoader::getMeshMaterial(int meshIndex) const
{
	return mMeshDataDefinitions[meshIndex]->material;
}

/*
 * Create mesh data streams at the component level.
 *
 */
void AssImpModelLoader::createMeshDataStreams()
{
	Importer importer;

	unsigned int pFlags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_SplitLargeMeshes;
	if (mSpecification.verticesIndexed())
	{
		pFlags |= aiProcess_JoinIdenticalVertices;
	}

	aiPropertyStore* props = aiCreatePropertyStore();

	if (mMaxVerticesPerMesh != (uint32)-1)
	{
		aiSetImportPropertyInteger(props, AI_CONFIG_PP_SLM_VERTEX_LIMIT, mMaxVerticesPerMesh);
	}

	aiScene const* scene = aiImportFileExWithProperties(mFilename.c_str(), pFlags, nullptr, props);
	aiReleasePropertyStore(props);

	if (!scene)
	{
		throw exception(("AssimpModelStream::loadImpl() could not load " + mFilename).c_str());
	}

	for (uint32 j = 0; j < scene->mNumMeshes; ++j)
	{
		// Create data streams from buffer definition, to be fed into the MeshDefinitions.
		aiMesh const* inputMesh = scene->mMeshes[j];

		MeshDataStreamDefinition* dataStreamDef = new MeshDataStreamDefinition();
		mMeshDataDefinitions.push_back(dataStreamDef);

		dataStreamDef->triangleCount = inputMesh->mNumFaces;

		// Name
		dataStreamDef->name = inputMesh->mName.C_Str();
		if (dataStreamDef->name == "")
		{
			dataStreamDef->name = utils::StringUtils::toString(j);
		}

		// Material
		auto material = scene->mMaterials[inputMesh->mMaterialIndex];
		aiString matName;
		
		material->Get(AI_MATKEY_NAME, matName);
		string matNameStr(matName.C_Str());

		if (matNameStr == "DefaultMaterial")
		{
			cout << "Warning: material used is 'DefaultMaterial'.  Check that the material library is both specified and enabled." << endl;
		}

		// Does this material already exist?
		if (mMaterialDefinitions.find(matNameStr) == mMaterialDefinitions.end())
		{
			// Create new material
			cout << "Parsing material '" << matNameStr << "'" << endl;
			MaterialDefinition matDef = createMaterialDefinition(matNameStr, material);
			mMaterialDefinitions[matNameStr] = matDef;
		}

		dataStreamDef->material = mMaterialDefinitions[matNameStr].name;

		int vertexStride = 0;

		// Positions
		bool hasPositions = inputMesh->HasPositions();
		if (hasPositions)
		{
			vertexStride += 3;
		}

		// Normals
		bool hasNormals = inputMesh->HasNormals();
		if (hasNormals)
		{
			vertexStride += 3;
		}

		// TexCoords
		bool hasTexCoords = inputMesh->HasTextureCoords(0);
		if (hasTexCoords)
		{
			vertexStride += 2;
		}

		bool hasColours = inputMesh->HasVertexColors(0);
		if (hasColours || mGenerateColours)
		{
			vertexStride += 4;
		}

		vector<float> vertices;
		vector<uint32> indices;

		if (inputMesh->mNumVertices <= numeric_limits<uint16>::max())
		{
			dataStreamDef->indexWidth = 16;
		}
		else
		{
			dataStreamDef->indexWidth = 32;
		}

		if (mSpecification.verticesIndexed())
		{
			// We want to use 16-bit indices for efficiency, but if the model
			// has more than 2^16 vertices then we have a problem.  Either we
			// split the model into multiple meshes, or use uint32 for the indices.

			// Copy the vertex data straight.
			for (uint32 k = 0; k < inputMesh->mNumVertices; ++k)
			{
				addBuildVertex(inputMesh, k, material, vertices, hasPositions, hasNormals, hasTexCoords, hasColours);
			}

			// Copy over index data
			for (uint32 k = 0; k < inputMesh->mNumFaces; ++k)
			{
				aiFace const& face = inputMesh->mFaces[k];
				addBuildFace(face.mIndices[0], face.mIndices[1], face.mIndices[2], indices);
			}
		}
		else
		{
			// Iterate over face list and create vertices from them.
			for (uint32 k = 0; k < inputMesh->mNumFaces; k++)
			{
				aiFace const& face = inputMesh->mFaces[k];

				for (int l = 0; l < 3; ++l)
				{
					addBuildVertex(inputMesh, face.mIndices[l], material, vertices, hasPositions, hasNormals, hasTexCoords, hasColours);
				}
			}
		}

		int srcVertexDataSize = vertices.size() * sizeof(float);

		float* dataPtr = new float[vertices.size()];
		memcpy(dataPtr, &(vertices[0]), srcVertexDataSize);

		dataStreamDef->vertexCount = (int)vertices.size() / vertexStride;

		int vertexOffset = 0;
		if (hasPositions)
		{
			VertexDataStreamDefinition vertexStreamDef;

			vertexStreamDef.data = (int8*)dataPtr;
			vertexStreamDef.dataType = Vertex::DataType::Float;
			vertexStreamDef.offset = vertexOffset;
			vertexStreamDef.stride = vertexStride;

			dataStreamDef->componentStreams[Vertex::Component::Position2] = vertexStreamDef;
			dataStreamDef->componentStreams[Vertex::Component::Position3] = vertexStreamDef;
			vertexOffset += mpp::mesh::Vertex::getDataTypeSize(vertexStreamDef.dataType) * 3;
		}

		if (hasNormals)
		{
			VertexDataStreamDefinition vertexStreamDef;

			vertexStreamDef.data = (int8*)dataPtr;
			vertexStreamDef.dataType = Vertex::DataType::Float;
			vertexStreamDef.offset = vertexOffset;
			vertexStreamDef.stride = vertexStride;

			dataStreamDef->componentStreams[Vertex::Component::Normal3] = vertexStreamDef;
			vertexOffset += mpp::mesh::Vertex::getDataTypeSize(vertexStreamDef.dataType) * 3;
		}

		if (hasTexCoords)
		{
			VertexDataStreamDefinition vertexStreamDef;

			vertexStreamDef.data = (int8*)dataPtr;
			vertexStreamDef.dataType = Vertex::DataType::Float;
			vertexStreamDef.offset = vertexOffset;
			vertexStreamDef.stride = vertexStride;

			dataStreamDef->componentStreams[Vertex::Component::TexCoord2] = vertexStreamDef;
			vertexOffset += mpp::mesh::Vertex::getDataTypeSize(vertexStreamDef.dataType) * 2;
		}

		if (hasColours || mGenerateColours)
		{
			VertexDataStreamDefinition vertexStreamDef;

			vertexStreamDef.data = (int8*)dataPtr;
			vertexStreamDef.dataType = Vertex::DataType::Float;
			vertexStreamDef.offset = vertexOffset;
			vertexStreamDef.stride = vertexStride;

			dataStreamDef->componentStreams[Vertex::Component::Colour1] = vertexStreamDef;
			dataStreamDef->componentStreams[Vertex::Component::Colour3] = vertexStreamDef;
			dataStreamDef->componentStreams[Vertex::Component::Colour4] = vertexStreamDef;
			vertexOffset += mpp::mesh::Vertex::getDataTypeSize(vertexStreamDef.dataType) * 4;
		}

		// Set index data
		if (mSpecification.verticesIndexed())
		{
			int indexWidthBytes = (dataStreamDef->indexWidth / 8);
			size_t dataSize = dataStreamDef->triangleCount * 3 * indexWidthBytes;
			dataStreamDef->indexData.reserve(dataSize);

			for (uint32 i = 0, j = 0; i < dataSize; i += indexWidthBytes, ++j)
			{
				uint32 index = indices[j];
				if (dataStreamDef->indexWidth == 16)
				{
					dataStreamDef->indexData.push_back(index & 255);
					dataStreamDef->indexData.push_back((index >> 8) & 255);
				}
				else
				{
					dataStreamDef->indexData.push_back(index & 255);
					dataStreamDef->indexData.push_back((index >> 8) & 255);
					dataStreamDef->indexData.push_back((index >> 16) & 255);
					dataStreamDef->indexData.push_back(index >> 24);
				}
			}
		}
	}

	aiReleaseImport(scene);
}

/*
 * Get model specification.
 *
 */
MeshSpecification& AssImpModelLoader::getMeshSpecification()
{
	return mSpecification;
}

/*
 * Create a mesh definition.
 *
 */
MeshDefinition* AssImpModelLoader::createMeshDefinition(int triangleCount, std::string const& name, string const& material, int indexWidth)
{
	MeshDefinition* md = new MeshDefinition(material, mpp::mesh::Primitive::Type::Triangles, mpp::mesh::VertexBufferStorageType::Static, triangleCount, indexWidth);
	mMeshDefinitions.push_back(md);

	return md;
}

/*
 * Create a material definition from an imported material.
 *
 */
MaterialDefinition AssImpModelLoader::createMaterialDefinition(string const& name, aiMaterial* material)
{
	MaterialDefinition matDef;

	matDef.name = name;
	
	// Find the appropriate information based on name
	auto it = find_if(mMaterialInfo.begin(), mMaterialInfo.end(), [name](auto kvp)
	{
		return regex_match(name, regex(kvp.first));
	});

	mpp::mesh_specification_parser::MaterialInformation mi;
	if (it == mMaterialInfo.end())
	{
		auto defIt = mMaterialInfo.find("");
		if (defIt == mMaterialInfo.end())
		{
			// Can't find a material to use
			string errMsg = "Cannot find a material specification for '" + name + "'.";
			throw exception(errMsg.c_str());
		}
		else
		{
			mi = defIt->second;
		}
	}
	else
	{
		mi = it->second;
	}

	matDef.program = mi.getProgram();

	// Get program info
	auto pit = mProgramInfo.find(matDef.program);
	if (pit == mProgramInfo.end())
	{
		string errMsg = "Cannot find a program specification '" + matDef.program + "'.";
		throw exception(errMsg.c_str());
	}

	mpp::mesh_specification_parser::ProgramInformation pi = pit->second;

	// Get texture and uniform information
	map<string, MaterialTransformer> matTokens =
	{
		{ "Material.Diffuse.Map", [](aiMaterial* mat, int index) 
			{ 
				aiString texName;
				mat->Get(AI_MATKEY_TEXTURE_DIFFUSE(index), texName); 
				return string(texName.C_Str());
			} 
		}
	};

	auto const& textures = pi.getTextures();

	for (auto kvp: textures)
	{
		string binding = kvp.first;
		string value = kvp.second;

		// Parse value
		utils::StringUtils::trim(value);
		if (utils::StringUtils::startsWith(value, "${") && utils::StringUtils::endsWith(value, "}"))
		{
			string token = value.substr(2, value.length() - 3);

			// Check token against recognised list.
			auto it = find_if(matTokens.begin(), matTokens.end(), [token](auto kvp)
			{
				return utils::StringUtils::startsWith(token, kvp.first);
			});

			if (it == matTokens.end())
			{
				string errMsg = "Unrecognised token '" + token + "'";
				throw exception(errMsg.c_str());
			}

			int index = token.find_last_of('.');
			string repl = it->second(material, utils::StringUtils::parseUInt(token.substr(index + 1)));

			matDef.textureBindings[kvp.first] = repl;
		}
		else
		{
			matDef.textureBindings[kvp.first] = kvp.second;
		}
	}

	return matDef;
}

/*
 * Get the number of mesh definitions.
 *
 */
int AssImpModelLoader::getNumMeshDefinitions() const
{
	return (int)mMeshDefinitions.size();
}

/*
 * Get specified mesh definition.
 *
 */
MeshDefinition* AssImpModelLoader::getMeshDefinition(int index)
{
	assert((index >= 0 && index < getNumMeshDefinitions()) && "ModelStream::getMeshDefinition() 'index' argument out of range!");
	return mMeshDefinitions[index];
}

/*
 * See if streams are tightly packed.  This lets us load in one big transfer.
 *
 */
bool AssImpModelLoader::streamsAreTightlyPacked(VertexBufferAttributeLayout const& bufferSpec, map<Vertex::Component, VertexDataStreamDefinition> const& componentStreams)
{
	int8 const* streamData1 = (componentStreams.begin())->second.data;
	int streamStride1 = (componentStreams.begin())->second.stride;

	for (auto it = componentStreams.begin(); it != componentStreams.end(); ++it)
	{
		// Are the sources the same?
		if (it->second.data != streamData1)
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
	for (int i = 0; i < bufferSpec.getNumAttributes(); ++i)
	{
		auto const& attrib = bufferSpec.getAttribute(i);
		auto const& stream = componentStreams.at(attrib.component);
		if (stream.dataType != attrib.dataType)
		{
			return false;
		}

		if (stream.offset != vertexStride)
		{
			return false;
		}

		vertexStride += Vertex::getComponentSize(attrib.component) + attrib.paddingBytes;
	}

	if (vertexStride != streamStride1)
	{
		return false;
	}

	return true;
}

/*
 * Copy data.
 *
 */
int8* AssImpModelLoader::copyVertexBufferData(VertexBufferAttributeLayout const& bufferSpec, VertexDataStreamDefinition componentStream, int vertexCount, int vertexStride)
{
	int8* bufData = new int8[vertexStride * vertexCount];

	memcpy(bufData, componentStream.data, vertexCount * vertexStride);
	return bufData;
}

/*
 * Split interlaced vertex data into one stream.
 *
 */
int8* AssImpModelLoader::deinterlaceVertexBufferData(VertexBufferAttributeLayout const& bufferSpec, map<Vertex::Component, VertexDataStreamDefinition> const& componentStreams, int vertexCount, int vertexStride)
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
			case Vertex::DataType::UnsignedByte:
			case Vertex::DataType::HalfFloat:
				for (int k = 0; k < vertexWidth; ++k)
				{
					float value = ((float const*)stream.data)[vertexOffset + k];
					if (attrib.dataType == Vertex::DataType::UnsignedByte)
					{
						// TODO: this assumes we have a normalised float.
						*bufDataPtr = (uint8)(value * 255.0f);
						bufDataPtr += sizeof(uint8);
					}
					else if (attrib.dataType == Vertex::DataType::HalfFloat)
					{
						uint16 hv16 = ((half_float::half)value).data_;
						*bufDataPtr++ = (uint8)(hv16 & 0xff);
						*bufDataPtr++ = (uint8)(hv16 >> 8);
					}
				}
				break;

			case Vertex::DataType::Float:
				memcpy(bufDataPtr, stream.data + vertexOffset * Vertex::getDataTypeSize(attrib.dataType), vertexWidth * Vertex::getDataTypeSize(attrib.dataType));
				bufDataPtr += vertexWidth * Vertex::getDataTypeSize(attrib.dataType);
				break;

			default:
				throw exception("ModelStream::loadImpl() cannot convert data to unsupported type!");
			}

			// Padding
			for (int k = 0; k < attrib.paddingBytes; ++k)
			{
				*bufDataPtr++ = 0;
			}
		}
	}

	return bufData;
}

/*
 * Load model.
 *
 */
void AssImpModelLoader::load()
{
	createMeshDataStreams();
	MeshSpecification& meshSpec = getMeshSpecification();

	// Create MeshDefinitions from data streams
	for (int i = 0; i < getNumMeshes(); ++i)
	{
		int triangleCount, vertexCount;
		getMeshCounts(i, &triangleCount, &vertexCount);

		MeshDefinition* meshDef = createMeshDefinition(triangleCount, getMeshName(i), getMeshMaterial(i), getMeshIndexWidth(i));

		// Set index data if we have any.
		vector<uint8> const& indexData = getMeshIndexData(i);
		if (indexData.size() && meshSpec.verticesIndexed())
		{
			int indexWidth = getMeshIndexWidth(i);
			int indexWidthBytes = indexWidth / 8;

			size_t dataSize = triangleCount * 3 * indexWidthBytes;

			uint8* indexBuffer = new uint8[dataSize];
			memcpy(indexBuffer, &(indexData[0]), dataSize);

			meshDef->setIndexData(shared_ptr<const uint8>(indexBuffer, [](uint8 *p) { delete[] p; }));
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

			// If primitive type is not triangles, convert it.
			auto primitiveType = meshSpec.getPrimitiveType();
			if (primitiveType != mpp::mesh::Primitive::Type::Triangles)
			{
				THROW_MPP_NOTIMP("conversion to non-triangle primitives", __LINE__, __FILE__, __func__);

				if (primitiveType == mpp::mesh::Primitive::Type::Points)
				{
					// Remove any index data
					meshDef->setIndexData(nullptr);
					meshDef->setNumPrimitives(meshDef->getNumPrimitives() * 3);
					meshSpec.setIndexedVertices(false);
				}
				else if (primitiveType == mpp::mesh::Primitive::Type::Lines)
				{
					// To do...
					// Convert each 3 vertices abc into 6 vertices abbcca.  This will be inefficient as many lines will be duplicated.
					//meshDef->setNumPrimitives(meshDef->getNumPrimitives() * 3);

				}
			}

			VertexBufferDefinition* vertexBufDef = meshDef->createVertexBufferDefinition(bufferSpec, vertexCount, vertexStride, shared_ptr<const int8>(bufData, [](int8 const* p) { delete[] p; }));
		}
	}
}

/*
 * Write out material definitions to a file.
 *
 */
void AssImpModelLoader::writeMaterials(string const& filename)
{
	utils::XmlWriter writer("Resources");
	auto rootNode = writer.getRootNode();

	for (auto it: mMaterialDefinitions)
	{
		auto matDef = it.second;

		// Material node
		auto materialNode = rootNode->createChild("Material");
		materialNode->addAttribute("name", matDef.name);

		// Program node
		auto programNode = materialNode->createChild("Program");
		programNode->addAttribute("name", matDef.program);

		// Textures
		auto texturesNode = materialNode->createChild("Textures");
		for (auto textureBinding: matDef.textureBindings)
		{
			auto textureNode = texturesNode->createChild("Texture");
			textureNode->addAttribute("binding", textureBinding.first);
			textureNode->addAttribute("resource", textureBinding.second);
		}

		// Uniforms
		auto uniformsNode = materialNode->createChild("Uniforms");
		for (auto uniform: matDef.uniformValues)
		{
			auto uniformNode = uniformsNode->createChild("Uniform");
			uniformNode->addAttribute("name", uniform.first);
			uniformNode->addAttribute("value", uniform.second);
		}
	}

	writer.write(filename);
}