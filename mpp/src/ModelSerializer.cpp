#include "mpp/ModelSerializer.h"
#include "mpp/MppException.h"

#define FLAG_INDEXED_VERTICES 0x0001

using namespace std;

namespace mpp
{
	using namespace mpp::mesh;

	/*
	 * Constructor.
	 *
	 */
	ModelSerializer::ModelSerializer()
	{
	}

	void ModelSerializer::clear()
	{
		mMaterialNames.clear();
		mMaterials.clear();
		mMeshSpecifications.clear();
		mVertexStreams.clear();
		mIndexStreamLookup.clear();
		mIndexStreams.clear();
		mMeshes.clear();
	}

	void ModelSerializer::writeValue(string const& value, ofstream& fp)
	{
		size_t len = value.length();
		fp.write((char const*)&len, sizeof(len));

		if (len > 0)
		{
			fp.write(value.c_str(), len);
		}
	}

	void ModelSerializer::writeValue(char const* value, size_t count, ofstream& fp)
	{
		if (count > 0)
		{
			fp.write(value, count);
		}
	}

	void ModelSerializer::writeValue(int32_t value, ofstream& fp)
	{
		fp.write((char const*)&value, sizeof(value));
	}

	void ModelSerializer::writeValue(uint32_t value, ofstream& fp)
	{
		auto pos = fp.tellp();
		fp.write((char const*)&value, sizeof(value));
	}

	void ModelSerializer::writeValue(uint16_t value, ofstream& fp)
	{
		fp.write((char const*)&value, sizeof(value));
	}

	void ModelSerializer::writeValue(int16_t value, ofstream& fp)
	{
		fp.write((char const*)&value, sizeof(value));
	}

	void ModelSerializer::writeValue(float value, ofstream& fp)
	{
		fp.write((char const*)&value, sizeof(value));
	}

	void ModelSerializer::writeValue(bool value, ofstream& fp)
	{
		char vi = value ? 1 : 0;
		fp.write((char const*)&vi, sizeof(vi));
	}

	string ModelSerializer::readString(ifstream& fp)
	{
		size_t len;
		fp.read((char*)&len, sizeof(len));

		if (len > 0)
		{
			char* buffer = new char[len + 1];
			fp.read(buffer, len);

			buffer[len] = '\0';

			string value(buffer);
			delete[] buffer;

			return value;
		}
		else
		{
			return "";
		}
	}

	void ModelSerializer::readBytes(int8_t* buffer, size_t count, ifstream& fp)
	{
		if (count > 0)
		{
			fp.read((char*)buffer, count);
		}
	}

	size_t ModelSerializer::readSize(ifstream& fp)
	{
		uint32_t value;

		fp.read((char*)&value, sizeof(value));
		return value;
	}

	int32_t ModelSerializer::readInt32(ifstream& fp)
	{
		int32_t value;

		fp.read((char*)&value, sizeof(value));
		return value;
	}

	uint32_t ModelSerializer::readUInt32(ifstream& fp)
	{
		uint32_t value;

		fp.read((char*)&value, sizeof(value));
		return value;
	}

	int16_t ModelSerializer::readInt16(ifstream& fp)
	{
		int16_t value;

		fp.read((char*)&value, sizeof(value));
		return value;
	}

	uint16_t ModelSerializer::readUInt16(ifstream& fp)
	{
		uint16_t value;

		fp.read((char*)&value, sizeof(value));
		return value;
	}

	float ModelSerializer::readFloat(ifstream& fp)
	{
		float value;

		fp.read((char*)&value, sizeof(value));
		return value;
	}

	bool ModelSerializer::readBool(ifstream& fp)
	{
		char value;

		fp.read((char*)&value, sizeof(value));
		return value != 0;
	}

	/*
	 * Read Header
	 *
	 */
	void ModelSerializer::readHeader(ifstream& fp)
	{
		/*
		4 bytes: id: value 'MMPM'
		2 bytes: version major
		2 bytes: version minor
		4 bytes: flags (including if indexed, etc).
		*/

		// Id
		char magic[4];
		readBytes((int8_t*)magic, 4, fp);

		if (magic[0] != 'M' || magic[1] != 'P' || magic[2] != 'P' || magic[3] != 'M')
		{
			THROW_MPP_IO("File is not a valid model file.", __LINE__, __FILE__, __func__);
		}

		// Version major
		mHeader.versionMajor = readUInt16(fp);
		mHeader.versionMinor = readUInt16(fp);

		// Flags
		mHeader.flags = readUInt32(fp);
	}

	/*
	 * Write Header
	 *
	 */
	void ModelSerializer::writeHeader(ofstream& fp)
	{
		/*
		4 bytes: id: value 'MMPM'
		2 bytes: version major
		2 bytes: version minor
		4 bytes: flags (including if indexed, etc).
		*/

		// Id
		char magic[4] = { 'M', 'P', 'P', 'M' };
		writeValue(magic, 4, fp);

		// Version major
		writeValue((uint16_t)1, fp);

		// Version minor
		writeValue((uint16_t)1, fp);

		// Flags
		uint32_t flags = 0;
		flags |= FLAG_INDEXED_VERTICES;

		writeValue(flags, fp);
	}

	/*
	 * Read directory.
	 *
	 */
	void ModelSerializer::readDirectory(ifstream& fp)
	{
		mDirectory.entries[(size_t)Directory::Entry::Type::Unused] = readDirectoryEntry(fp);
		mDirectory.entries[(size_t)Directory::Entry::Type::MaterialNames] = readDirectoryEntry(fp);
		mDirectory.entries[(size_t)Directory::Entry::Type::Materials] = readDirectoryEntry(fp);
		mDirectory.entries[(size_t)Directory::Entry::Type::MeshSpecifications] = readDirectoryEntry(fp);
		mDirectory.entries[(size_t)Directory::Entry::Type::VertexData] = readDirectoryEntry(fp);
		mDirectory.entries[(size_t)Directory::Entry::Type::IndexData] = readDirectoryEntry(fp);
		mDirectory.entries[(size_t)Directory::Entry::Type::MeshMetadata] = readDirectoryEntry(fp);
	}

	/*
	 * Read directory entry.
	 *
	 */
	ModelSerializer::Directory::Entry ModelSerializer::readDirectoryEntry(ifstream& fp)
	{
		ModelSerializer::Directory::Entry entry;

		entry.type = static_cast<Directory::Entry::Type>(readUInt32(fp));
		entry.startOffset = readUInt32(fp);
		entry.endOffset = readUInt32(fp);
		entry.count = readSize(fp);

		return entry;
	}

	void ModelSerializer::updateDirectoryEntry(ofstream& fp, Directory::Entry::Type type, size_t start, size_t end, size_t count)
	{
		auto oldPos = fp.tellp();

		size_t newPos = sizeof(Header) + (size_t)type * sizeof(Directory::Entry);

		fp.seekp((streampos)newPos);
		writeDirectoryEntry(fp, { type, start, end, count });

		fp.seekp(oldPos);
	}

	/*
	 * Write directory.
	 *
	 */
	void ModelSerializer::writeDirectory(ofstream& fp)
	{
		writeDirectoryEntry(fp, { Directory::Entry::Type::Unused, 0, 0, 0 });
		writeDirectoryEntry(fp, { Directory::Entry::Type::MaterialNames, 0, 0, mMaterialNames.size() });
		writeDirectoryEntry(fp, { Directory::Entry::Type::Materials, 0, 0, mMaterials.size() });
		writeDirectoryEntry(fp, { Directory::Entry::Type::MeshSpecifications, 0, 0, mMeshes.size() });
		writeDirectoryEntry(fp, { Directory::Entry::Type::VertexData, 0, 0, 0 });
		writeDirectoryEntry(fp, { Directory::Entry::Type::IndexData, 0, 0, 0 });
		writeDirectoryEntry(fp, { Directory::Entry::Type::MeshMetadata, 0, 0, 0 });
	}

	/*
	 * Write directory entry.
	 *
	 */
	void ModelSerializer::writeDirectoryEntry(ofstream& fp, Directory::Entry const& entry)
	{
		writeValue((uint32_t)entry.type, fp);
		writeValue(entry.startOffset, fp);
		writeValue(entry.endOffset, fp);
		writeValue(entry.count, fp);
	}

	void ModelSerializer::readMaterialNames(ifstream& fp)
	{
		auto const& entry = mDirectory.entries[(int)Directory::Entry::Type::MaterialNames];

		fp.seekg(entry.startOffset);

		for (size_t i = 0; i < entry.count; ++i)
		{
			mMaterialNames.push_back(readString(fp));
		}

		if (fp.tellg() != entry.endOffset)
		{
			THROW_MPP("Invalid file position.", __LINE__, __FILE__, __func__);
		}
	}

	void ModelSerializer::readMaterials(ifstream& fp)
	{
		auto const& entry = mDirectory.entries[(int)Directory::Entry::Type::Materials];

		fp.seekg(entry.startOffset);

		for (size_t i = 0; i < entry.count; ++i)
		{
			mMaterials.push_back(readMaterial(fp));
		}

		if (fp.tellg() != entry.endOffset)
		{
			THROW_MPP("Invalid file position.", __LINE__, __FILE__, __func__);
		}
	}

	MaterialSpecification ModelSerializer::readMaterial(ifstream& fp)
	{
		MaterialSpecification matSpec;

		// Program options
		matSpec.program.resourceExists = readBool(fp);
		matSpec.program.existingResource = readString(fp);
		matSpec.program.isChild = readBool(fp);
		matSpec.program.is2d = readBool(fp);

		// MeshSpecification
		matSpec.program.spec = readMeshSpecification(fp);

		// Shaders
		matSpec.program.vertexShader.type = static_cast<MaterialSpecification::ProgramOptions::Shader::Type>(readUInt32(fp));
		matSpec.program.vertexShader.data = readString(fp);

		matSpec.program.geometryShader.type = static_cast<MaterialSpecification::ProgramOptions::Shader::Type>(readUInt32(fp));
		matSpec.program.geometryShader.data = readString(fp);

		matSpec.program.fragmentShader.type = static_cast<MaterialSpecification::ProgramOptions::Shader::Type>(readUInt32(fp));
		matSpec.program.fragmentShader.data = readString(fp);

		// Uniforms
		matSpec.uniforms = readUniformCollection(fp);

		// Textures
		size_t numTextures = readUInt32(fp);
		for (size_t i = 0; i < numTextures; ++i)
		{
			MaterialSpecification::TextureOptions textureOptions;

			textureOptions.resourceExists = readBool(fp);
			textureOptions.existingResource = readString(fp);
			textureOptions.isChild = readBool(fp);
			textureOptions.sampler = readString(fp);
			textureOptions.source = readString(fp);

			// TextureParams
			textureOptions.params.minFilter = readUInt32(fp);
			textureOptions.params.magFilter = readUInt32(fp);
			textureOptions.params.wrap = readUInt32(fp);
			textureOptions.params.useMipmaps = readBool(fp);
			textureOptions.params.lodBaseLevel = readInt32(fp);
			textureOptions.params.lodMaxLevel = readInt32(fp);
			textureOptions.params.lodBias = readFloat(fp);
			textureOptions.params.maxAnisotropy = readFloat(fp);
		
			matSpec.textures.push_back(textureOptions);
		}

		return matSpec;
	}

	void ModelSerializer::writeUniformCollection(UniformCollection const& uniforms, ofstream& fp)
	{
		writeValue(uniforms.getNumUniforms(), fp);

		auto const& uniformData = uniforms.getUniformData();

		for (auto const& kvp : uniformData)
		{
			auto const& data = kvp.second;

			writeValue(data.name, fp);
			writeValue((uint32_t)data.type, fp);
			writeValue(data.size, fp);
			writeValue(data.data, 64, fp);
		}
	}

	UniformCollection ModelSerializer::readUniformCollection(ifstream& fp)
	{
		UniformCollection uniforms;

		size_t numUniforms = readUInt32(fp);
		for (size_t i = 0; i < numUniforms; ++i)
		{
			string name = readString(fp);
			auto type = static_cast<program::GLSLType>(readUInt32(fp));
			auto size = readSize(fp);

			char data[64];
			fp.read(data, 64);

			uniforms.setUniform(name, type, size, data);
		}

		return uniforms;
	}

	void ModelSerializer::writeMaterialNames(ofstream& fp)
	{
		auto start = (size_t)fp.tellp();

		for (auto const& name: mMaterialNames)
		{
			writeValue(name, fp);
		}

		auto end = (size_t)fp.tellp();
		updateDirectoryEntry(fp, Directory::Entry::Type::MaterialNames, start, end, mMaterialNames.size());
	}


	/*
	 * Write all materials.
	 *
	 */
	void ModelSerializer::writeMaterials(ofstream& fp)
	{
		auto start = (size_t)fp.tellp();

		// Write and update mapping
		for (auto const& material: mMaterials)
		{
			writeMaterial(material, fp);
		}

		auto end = (size_t)fp.tellp();
		updateDirectoryEntry(fp, Directory::Entry::Type::Materials, start, end, mMaterials.size());
	}

	void ModelSerializer::writeMaterial(MaterialSpecification const& matSpec, ofstream& fp)
	{
		// Program options
		writeValue(matSpec.program.resourceExists, fp);
		writeValue(matSpec.program.existingResource, fp);
		writeValue(matSpec.program.isChild, fp);
		writeValue(matSpec.program.is2d, fp);

		writeMeshSpecification(matSpec.program.spec, fp);

		// Shaders
		writeValue((uint32_t)matSpec.program.vertexShader.type, fp);
		writeValue(matSpec.program.vertexShader.data, fp);

		writeValue((uint32_t)matSpec.program.geometryShader.type, fp);
		writeValue(matSpec.program.geometryShader.data, fp);

		writeValue((uint32_t)matSpec.program.fragmentShader.type, fp);
		writeValue(matSpec.program.fragmentShader.data, fp);

		// Uniforms
		writeUniformCollection(matSpec.uniforms, fp);

		// Textures
		writeValue(matSpec.textures.size(), fp);
		for (auto const& texture: matSpec.textures)
		{
			writeValue(texture.resourceExists, fp);
			writeValue(texture.existingResource, fp);
			writeValue(texture.isChild, fp);
			writeValue(texture.sampler, fp);
			writeValue(texture.source, fp);

			// TextureParams
			writeValue(texture.params.minFilter, fp);
			writeValue(texture.params.magFilter, fp);
			writeValue(texture.params.wrap, fp);
			writeValue(texture.params.useMipmaps, fp);
			writeValue(texture.params.lodBaseLevel, fp);
			writeValue(texture.params.lodMaxLevel, fp);
			writeValue(texture.params.lodBias, fp);
			writeValue(texture.params.maxAnisotropy, fp);
		}
	}

	void ModelSerializer::readMeshSpecifications(ifstream& fp)
	{
		auto const& entry = mDirectory.entries[(int)Directory::Entry::Type::MeshSpecifications];

		fp.seekg(entry.startOffset);

		for (size_t i = 0; i < entry.count; ++i)
		{
			mMeshSpecifications.push_back(readMeshSpecification(fp));
		}

		if (fp.tellg() != entry.endOffset)
		{
			THROW_MPP("Invalid file position.", __LINE__, __FILE__, __func__);
		}
	}

	/*
	 * Read model specification
	 *
	 */
	MeshSpecification ModelSerializer::readMeshSpecification(ifstream& fp)
	{
		/*
		4 bytes: primitive type
		4 bytes: storage type
		1 byte : indexed vertices?
		2 bytes: layout count
		<buffer count> times: buffer data
		*/
		mesh::MeshSpecification meshSpec;

		auto primitiveType = static_cast<mesh::Primitive::Type>(readUInt32(fp));
		meshSpec.setPrimitiveType(primitiveType);

		auto storageType = static_cast<mesh::VertexBufferStorageType>(readUInt32(fp));
		meshSpec.setStorageType(storageType);

		auto indexed = readBool(fp);
		meshSpec.setIndexedVertices(indexed);

		auto numLayouts = readSize(fp);
		for (size_t i = 0; i < numLayouts; ++i)
		{
			auto isStatic = readBool(fp);
			auto layout = meshSpec.createVertexBufferAttributeLayout(isStatic);

			auto numAttribs = readSize(fp);
			for (size_t j = 0; j < numAttribs; ++j)
			{
				auto component = static_cast<mesh::Vertex::Component>(readUInt32(fp));
				auto datatype = static_cast<mesh::Vertex::DataType>(readUInt32(fp));
				auto padToBoundary = readInt32(fp);
				auto normalised = readBool(fp);

				// Need to sort out paddingBytes
				layout->createAttribute(component, datatype, normalised, padToBoundary);
			}
		}

		return meshSpec;
	}

	/*
	 * Write all MeshSpecifications.
	 *
	 */
	void ModelSerializer::writeMeshSpecifications(ofstream& fp)
	{
		auto start = (size_t)fp.tellp();

		for (size_t i = 0; i < mMeshSpecifications.size(); ++i)
		{
			writeMeshSpecification(mMeshSpecifications[i], fp);
		}

		auto end = (size_t)fp.tellp();
		updateDirectoryEntry(fp, Directory::Entry::Type::MeshSpecifications, start, end, mMeshSpecifications.size());
	}

	/*
	 * Write mesh specification
	 *
	 */
	void ModelSerializer::writeMeshSpecification(MeshSpecification const& meshSpec, ofstream& fp)
	{
		/*
		4 bytes: primitive type
		4 bytes: storage type
		1 byte : indexed vertices?
		2 bytes: layout count
		<buffer count> times: buffer data
		*/
		writeValue((uint32_t)meshSpec.getPrimitiveType(), fp);
		writeValue((uint32_t)meshSpec.getStorageType(), fp);
		writeValue(meshSpec.verticesIndexed(), fp);

		// Write layouts
		writeValue(meshSpec.getNumVertexBufferAttributeLayouts(), fp);
		for (size_t i = 0; i < meshSpec.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto const& layout = meshSpec.getVertexBufferAttributeLayout(i);

			writeValue(layout.isStatic(), fp);

			// Write attributes
			writeValue(layout.getNumAttributes(), fp);
			for (size_t j = 0; j < layout.getNumAttributes(); ++j)
			{
				auto const& attrib = layout.getAttribute(j);

				writeValue((uint32_t)attrib.component, fp);
				writeValue((uint32_t)attrib.dataType, fp);
				writeValue(attrib.padToBoundary, fp);
				writeValue(attrib.normalised, fp);
			}
		}
	}

	void ModelSerializer::readVertexBuffers(ifstream& fp)
	{
		auto const& entry = mDirectory.entries[(int)Directory::Entry::Type::VertexData];

		fp.seekg(entry.startOffset);

		for (size_t i = 0; i < entry.count; ++i)
		{
			mVertexStreams.push_back(readVertexBuffer(fp));
		}

		if (fp.tellg() != entry.endOffset)
		{
			THROW_MPP("Invalid file position.", __LINE__, __FILE__, __func__);
		}
	}

	/*
	 * Read vertex buffer
	 *
	 */
	ModelSerializer::VertexStream ModelSerializer::readVertexBuffer(ifstream& fp)
	{
		/*
		4 bytes: vertex data size in bytes
		4 bytes: vertex count
		4 bytes: vertex stride
		x bytes: vertex data
		*/

		size_t vertexDataSize, vertexCount, vertexStride;

		vertexDataSize = readSize(fp);
		vertexCount = readSize(fp);
		vertexStride = readSize(fp);

		int8_t* vertexData = new int8_t[vertexDataSize];
		readBytes(vertexData, vertexDataSize, fp);

		VertexStream vs;
		vs.vertexCount = vertexCount;
		vs.vertexStride = vertexStride;
		vs.vertexData = shared_ptr<const int8_t>(vertexData, [](int8_t *p) { delete[] p; });

		return vs;
	}

	/*
	 * Write all Vertex Buffers.
	 *
	 */
	void ModelSerializer::writeVertexBuffers(ofstream& fp)
	{
		auto start = (size_t)fp.tellp();

		for (size_t i = 0; i < mVertexStreams.size(); ++i)
		{
			writeVertexBuffer(mVertexStreams[i], fp);
		}

		auto end = (size_t)fp.tellp();
		updateDirectoryEntry(fp, Directory::Entry::Type::VertexData, start, end, mVertexStreams.size());
	}

	/*
	 * Write vertex buffer
	 *
	 */
	void ModelSerializer::writeVertexBuffer(VertexStream const& vertexStream, ofstream& fp)
	{
		/*
		4 bytes: vertex data size in bytes
		4 bytes: vertex count
		4 bytes: vertex stride
		x bytes: vertex data
		*/

		size_t vertexDataSize = vertexStream.vertexCount * vertexStream.vertexStride;
		writeValue(vertexDataSize, fp);

		writeValue(vertexStream.vertexCount, fp);
		writeValue(vertexStream.vertexStride, fp);
		writeValue((char*)vertexStream.vertexData.get(), vertexDataSize, fp);
	}

	void ModelSerializer::readIndexBuffers(ifstream& fp)
	{
		auto const& entry = mDirectory.entries[(int)Directory::Entry::Type::IndexData];

		fp.seekg(entry.startOffset);

		for (size_t i = 0; i < entry.count; ++i)
		{
			mIndexStreams.push_back(readIndexBuffer(fp));
		}

		if (fp.tellg() != entry.endOffset)
		{
			THROW_MPP("Invalid file position.", __LINE__, __FILE__, __func__);
		}
	}

	ModelSerializer::IndexStream ModelSerializer::readIndexBuffer(ifstream& fp)
	{
		/*
		4 bytes: index data size in bytes
		4 bytes: index width
		x bytes: index data
		*/
		auto dataSize = readSize(fp);
		auto indexWidth = readSize(fp);

		uint8_t* indexData = new uint8_t[dataSize];
		readBytes((int8_t*)indexData, dataSize, fp);

		IndexStream is;
		is.indexWidth = indexWidth;
		is.indexData = shared_ptr<const uint8_t>(indexData, [](uint8_t *p) { delete[] p; });

		return is;
	}

	/*
	 * Write all Index Buffers.
	 *
	 */
	void ModelSerializer::writeIndexBuffers(ofstream& fp)
	{
		auto start = (size_t)fp.tellp();

		for (size_t i = 0; i < mIndexStreams.size(); ++i)
		{
			auto const& mesh = mMeshes[mIndexStreamLookup[i]];
			writeIndexBuffer(fp, mIndexStreams[i], mesh.primitiveType, mesh.primitiveCount);
		}

		auto end = (size_t)fp.tellp();
		updateDirectoryEntry(fp, Directory::Entry::Type::IndexData, start, end, mIndexStreams.size());
	}

	void ModelSerializer::writeIndexBuffer(ofstream& fp, IndexStream const& indexStream, Primitive::Type primitiveType, size_t numPrimitives)
	{
		/*
		4 bytes: index data size in bytes
		4 bytes: index width
		x bytes: index data
		*/
		size_t indexDataSize = numPrimitives *
			mesh::Primitive::size(primitiveType) *
			indexStream.indexWidth / 8;

		writeValue(indexDataSize, fp);
		writeValue(indexStream.indexWidth, fp);
		writeValue((char const*)indexStream.indexData.get(), indexDataSize, fp);
	}

	void ModelSerializer::readMeshes(ifstream& fp)
	{
		auto const& entry = mDirectory.entries[(int)Directory::Entry::Type::MeshMetadata];

		fp.seekg(entry.startOffset);

		for (size_t i = 0; i < entry.count; ++i)
		{
			mMeshes.push_back(readMesh(fp));
		}

		if (fp.tellg() != entry.endOffset)
		{
			THROW_MPP("Invalid file position.", __LINE__, __FILE__, __func__);
		}
	}

	/*
	 * Read mesh definition
	 *
	 */
	ModelSerializer::Mesh ModelSerializer::readMesh(ifstream& fp)
	{
		/*
		zero-str: name
		4 bytes: primitive type
		4 bytes: primitive count
		X bytes: material name
		4 bytes: index buffer id (or -1 for none)
		4 bytes: number of vertex buffers
		...    : vertex buffer ids (4 bytes each)
		*/
		Mesh mesh;

		mesh.name = readString(fp);

		mesh.primitiveType = static_cast<mesh::Primitive::Type>(readUInt32(fp));
		mesh.primitiveCount = readUInt32(fp);
		mesh.material = readString(fp);
		mesh.meshSpec = readUInt32(fp);

		auto numVertexBuffers = readSize(fp);

		for (size_t i = 0; i < numVertexBuffers; ++i)
		{
			auto vertexBufferId = readUInt32(fp);
			mesh.vertexStreams.push_back(vertexBufferId);
		}

		mesh.indexStream = readUInt32(fp);

		return mesh;
	}

	void ModelSerializer::writeMeshes(ofstream& fp)
	{
		auto start = (size_t)fp.tellp();

		for (uint32_t i = 0; i < mMeshes.size(); ++i)
		{
			writeMesh(fp, mMeshes[i], i);
		}

		auto end = (size_t)fp.tellp();
		updateDirectoryEntry(fp, Directory::Entry::Type::MeshMetadata, start, end, mMeshes.size());
	}

	/*
	 * Write mesh definition
	 *
	 */
	void ModelSerializer::writeMesh(ofstream& fp, Mesh const& mesh, uint32_t index)
	{
		/*
		zero-str: name
		4 bytes: primitive type
		4 bytes: primitive count
		4 bytes: material id
		4 bytes: index buffer id (or -1 for none)
		4 bytes: number of vertex buffers
		...    : vertex buffer ids (4 bytes each)
		*/
		writeValue(mesh.name, fp);
		
		// Write material, meshspec, index buffer and vertex buffer IDs
		writeValue((uint32_t)mesh.primitiveType, fp);
		writeValue(mesh.primitiveCount, fp);
		writeValue(mesh.material, fp);
		writeValue(mesh.meshSpec, fp);

		writeValue(mesh.vertexStreams.size(), fp);
		for (auto vb: mesh.vertexStreams)
		{
			writeValue(vb, fp);
		}

		writeValue(mesh.indexStream, fp);
	}

	/*
	 * Set name.
	 *
	 */
	void ModelSerializer::setName(size_t meshIndex, string const& name)
	{
		mMeshes[meshIndex].name = name;
	}

	/*
	 * Get name.
	 *
	 */
	string const& ModelSerializer::getName(size_t meshIndex) const
	{
		return mMeshes[meshIndex].name;
	}

	/*
	 * Set the number of meshes in this file.
	 *
	 */
	void ModelSerializer::setMeshCount(size_t count)
	{
		mMeshes.resize(count);
	}

	/*
	 * Get the number of meshes in this file.
	 *
	 */
	size_t ModelSerializer::getMeshCount() const
	{
		return mMeshes.size();
	}

	/*
	 * Set primitive type.
	 *
	 */
	void ModelSerializer::setPrimitiveType(size_t meshIndex, Primitive::Type primitiveType)
	{
		mMeshes[meshIndex].primitiveType = primitiveType;
	}

	/*
	 * Get primitive type.
	 *
	 */
	Primitive::Type ModelSerializer::getPrimitiveType(size_t meshIndex) const
	{
		return mMeshes[meshIndex].primitiveType;
	}

	/*
  	 * Set primitive count.
  	 *
	 */
	void ModelSerializer::setPrimitiveCount(size_t meshIndex, size_t primitiveCount)
	{
		mMeshes[meshIndex].primitiveCount = primitiveCount;
	}

	/*
	 * Get primitive count.
	 *
	 */
	int ModelSerializer::getPrimitiveCount(size_t meshIndex) const
	{
		return mMeshes[meshIndex].primitiveCount;
	}

	/*
	 * Set mesh specification.
	 *
	 */
	void ModelSerializer::setMeshSpecification(size_t meshIndex, MeshSpecification const& specification)
	{
		mMeshes[meshIndex].meshSpec = mMeshSpecifications.size();
		mMeshSpecifications.push_back(specification);
	}

	/*
	 * Get mesh specification.
	 *
	 */
	MeshSpecification const& ModelSerializer::getMeshSpecification(size_t meshIndex) const
	{
		return mMeshSpecifications[mMeshes[meshIndex].meshSpec];
	}

	void ModelSerializer::addMaterial(string const& name, MaterialSpecification const& matSpec)
	{
		mMaterialNames.push_back(name);
		mMaterials.push_back(matSpec);
	}

	/*
	 * Set material.
	 *
	 */
	void ModelSerializer::setMaterial(size_t meshIndex, string const& material)
	{
		mMeshes[meshIndex].material = material;
	}

	/*
	 * Get material.
	 *
	 */
	string const& ModelSerializer::getMaterial(size_t meshIndex) const
	{
		return mMeshes[meshIndex].material;
	}

	vector<string> const& ModelSerializer::getMaterialNames() const
	{
		return mMaterialNames;
	}

	vector<MaterialSpecification> const& ModelSerializer::getMaterials() const
	{
		return mMaterials;
	}

	/*
	 * Add a vertex stream.
	 *
	 */
	void ModelSerializer::addVertexStream(size_t meshIndex, size_t vertexCount, size_t vertexStride, std::shared_ptr<const int8_t> vertexData)
	{
		mMeshes[meshIndex].vertexStreams.push_back(mVertexStreams.size());

		VertexStream vs;
		vs.vertexCount = vertexCount;
		vs.vertexStride = vertexStride;
		vs.vertexData = vertexData;

		mVertexStreams.push_back(vs);
	}

	/*
	 * Get specified vertex stream.
	 *
	 */
	void ModelSerializer::getVertexStream(size_t meshIndex, size_t index, size_t* vertexCount, size_t* vertexStride, shared_ptr<const int8_t>* vertexData)
	{
		auto const& vertexStream = mVertexStreams[mMeshes[meshIndex].vertexStreams[index]];
		*vertexCount = vertexStream.vertexCount;
		*vertexStride = vertexStream.vertexStride;
		*vertexData = vertexStream.vertexData;
	}

	/*
	 * Set index data.
	 *
	 */
	void ModelSerializer::setIndexBuffer(size_t meshIndex, shared_ptr<const uint8_t> indexData, size_t indexWidth)
	{
		auto streamIndex = mIndexStreams.size();
		mMeshes[meshIndex].indexStream = streamIndex;
		mIndexStreamLookup[streamIndex] = meshIndex;

		IndexStream is;
		is.indexData = indexData;
		is.indexWidth = indexWidth;

		mIndexStreams.push_back(is);
	}

	/*
	 * Get index data.
	 *
	 */
	shared_ptr<const uint8_t> ModelSerializer::getIndexData(size_t meshIndex) const
	{
		return mIndexStreams[mMeshes[meshIndex].indexStream].indexData;
	}

	int ModelSerializer::getIndexWidth(size_t meshIndex) const
	{
		return mIndexStreams[mMeshes[meshIndex].indexStream].indexWidth;
	}

	/*
	 * Save file.
	 *
	 */
	void ModelSerializer::save(string const& filename)
	{
		ofstream fp(filename, ios::out | ios::binary);

		if (!fp.is_open())
		{
			THROW_MPP_IO("Could not open " + filename + " for writing.", __LINE__, __FILE__, __func__);
		}
		
		writeHeader(fp);
		writeDirectory(fp);

		writeMaterialNames(fp);
		writeMaterials(fp);
		writeMeshSpecifications(fp);
		writeVertexBuffers(fp);
		writeIndexBuffers(fp);
		writeMeshes(fp);
			
		fp.close();
	}

	/*
	 * Load file.
	 *
	 */
	void ModelSerializer::load(std::string const& filename)
	{
		clear();

		ifstream fp(filename, ios::in | ios::binary);

		if (!fp.is_open())
		{
			THROW_MPP_IO("Could not open " + filename + " for reading.", __LINE__, __FILE__, __func__);
		}

		readHeader(fp);
		readDirectory(fp);

		readMaterialNames(fp);
		readMaterials(fp);
		readMeshSpecifications(fp);
		readVertexBuffers(fp);
		readIndexBuffers(fp);
		readMeshes(fp);

		fp.close();
	}

	ModelSerializer::MetadataReader ModelSerializer::getReader(string const& filename)
	{
		// If we pass in a filename, then load it in.  Otherwise, assume the
		// data has already been loaded (either from a file, or specified from API).
		if (filename != "")
		{
			clear();

			ifstream fp(filename, ios::in | ios::binary);

			if (!fp.is_open())
			{
				THROW_MPP_IO("Could not open " + filename + " for reading.", __LINE__, __FILE__, __func__);
			}

			readHeader(fp);
			readDirectory(fp);

			readMaterialNames(fp);
			readMaterials(fp);
			readMeshSpecifications(fp);
			readMeshes(fp);

			fp.close();
		}

		MetadataReader reader(
			mMaterialNames,
			mMaterials,
			mMeshSpecifications);

		for (size_t i = 0; i < mMeshes.size(); ++i)
		{
			auto const& mesh = mMeshes[i];
			reader.addMesh(mesh.name, mesh.meshSpec, mesh.material, mesh.primitiveType);
		}

		return reader;
	}
}


