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
		mMaterialLookup.clear();
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

	string ModelSerializer::readBytes(size_t count, ifstream& fp)
	{
		if (count > 0)
		{
			char* buffer = new char[count + 1];
			fp.read(buffer, count);

			buffer[count] = '\0';

			string value(buffer);
			delete[] buffer;

			return value;
		}
		else
		{
			return "";
		}
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
		string magic = readBytes(4, fp);

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
		entry.count = readUInt32(fp);

		return entry;
	}

	void ModelSerializer::updateDirectoryEntry(ofstream& fp, Directory::Entry::Type type, streampos start, streampos end, size_t count)
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
		writeValue((uint32_t)entry.startOffset, fp);
		writeValue((uint32_t)entry.endOffset, fp);
		writeValue((uint32_t)entry.count, fp);
	}

	void ModelSerializer::readMaterials(ifstream& fp)
	{
		auto const& entry = mDirectory.entries[(int)Directory::Entry::Type::Materials];

		fp.seekg((streampos)entry.startOffset);

		for (size_t i = 0; i < entry.count; ++i)
		{
			auto material = readMaterial(fp);

			mMaterialLookup[material.getName()] = mMaterials.size();
			mMaterials.push_back(material);
		}

		if (fp.tellg != (streampos)entry.endOffset)
		{
			THROW_MPP("Invalid file position.", __LINE__, __FILE__, __func__);
		}
	}

	MaterialSpecification ModelSerializer::readMaterial(ifstream& fp)
	{
		auto name = readString(fp);

		MaterialSpecification matSpec;

		// Program options
		matSpec.program.resourceExists = readBool(fp);
		matSpec.program.existingResource = readString(fp);
		matSpec.program.isChild = readBool(fp);
		matSpec.program.is2d = readBool(fp);

		// MeshSpecification
		auto meshSpec = readMeshSpecification(fp);
		matSpec.program.spec = meshSpec;

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
		for (size_t j = 0; j < numTextures; ++j)
		{
			auto sampler = readString(fp);
			auto resource = readString(fp);
			auto exists = readBool(fp);

			matSpec.textures[sampler] = make_pair(resource, exists);
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

			fp.write(data.data, 64);
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
			auto size = readUInt32(fp);

			char data[64];
			fp.read(data, 64);

			uniforms.setUniform(name, type, size, data);
		}

		return uniforms;
	}


	/*
		* Write all materials.
		*
		*/
	void ModelSerializer::writeMaterials(ofstream& fp)
	{
		auto start = fp.tellp();

		// Write and update mapping
		for (size_t i = 0; i < mMaterials.size(); ++i)
		{
			writeMaterial(fp, mMaterials[i]);
		}

		auto end = fp.tellp();
		updateDirectoryEntry(fp, Directory::Entry::Type::Materials, start, end, mMaterials.size());
	}

	void ModelSerializer::writeMaterial(ofstream& fp, MaterialSpecification const& matSpec)
	{
		writeValue(name, fp);

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
		for (auto const& kvp : matSpec.textures)
		{
			writeValue(kvp.first, fp);
			writeValue(kvp.second.first, fp);
			writeValue(kvp.second.second, fp);
		}
	}

	void ModelSerializer::readMeshSpecifications(ifstream& fp)
	{
		auto const& entry = mDirectory.entries[(int)Directory::Entry::Type::MeshSpecifications];

		fseek(fp, entry.startOffset, SEEK_SET);

		for (size_t i = 0; i < entry.count; ++i)
		{
			mMeshSpecifications.push_back(readMeshSpecification(fp));
		}

		if (ftell(fp) != entry.endOffset)
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

		size_t numLayouts = readUInt32(fp);
		for (size_t i = 0; i < numLayouts; ++i)
		{
			auto isStatic = readBool(fp);

			auto layout = meshSpec.createVertexBufferAttributeLayout(isStatic);

			size_t numAttribs = readUInt32(fp);
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
		auto start = fp.tellp();

		for (uint32_t i = 0; i < mMeshSpecifications.size(); ++i)
		{
			writeMeshSpecification(mMeshSpecifications[i], fp);
		}

		auto end = fp.tellp();
		updateDirectoryEntry(fp, Directory::Entry::Type::MeshSpecifications, start, end, mMeshSpecifications.size());
	}

	/*
		* Write mesh specification
		*
		*/
	void ModelSerializer::writeMeshSpecification( MeshSpecification const& meshSpec, ofstream& fp)
	{
		/*
		4 bytes: primitive type
		4 bytes: storage type
		1 byte : indexed vertices?
		2 bytes: layout count
		<buffer count> times: buffer data
		*/
		auto const& ms = meshSpec;

		writeValue((uint32_t)ms.getPrimitiveType(), fp);
		writeValue((uint32_t)ms.getStorageType(), fp);
		writeValue(ms.verticesIndexed(), fp);

		// Write layouts
		writeValue(ms.getNumVertexBufferAttributeLayouts(), fp);
		for (int i = 0; i < ms.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto const& layout = ms.getVertexBufferAttributeLayout(i);

			writeValue(layout.isStatic(), fp);

			// Write attributes
			writeValue(layout.getNumAttributes(), fp);
			for (int j = 0; j < layout.getNumAttributes(); ++j)
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

		int vertexDataSize, vertexCount, vertexStride;

		fread(&vertexDataSize, sizeof(int), 1, fp);
		fread(&vertexCount, sizeof(int), 1, fp);
		fread(&vertexStride, sizeof(int), 1, fp);

		int8_t* vertexData = new int8_t[vertexDataSize];
		fread(vertexData, vertexDataSize, 1, fp);

		VertexStream vs;
		vs.vertexCount = vertexCount;
		vs.vertexStride = vertexStride;
		vs.vertexData = shared_ptr<const int8_t>(vertexData, [](int8_t *p) { delete[] p; });

		return vs;
	}

	/*
		* Read Vertex Layout
		*
		*/
	void ModelSerializer::readVertexBufferAttributeLayout(ifstream& fp, mpp::mesh::VertexBufferAttributeLayout* layout, uint32_t attribOffset)
	{
		/*
		2 bytes: attribute count

		<attrib count> times:
		4 bytes : component
		4 bytes : data type
		4 bytes : padding bytes
		4 bytes : normalised
		*/

		uint16_t attribCount;
		fread(&attribCount, sizeof(uint16_t), 1, fp);

		for (int i = 0; i < attribCount; ++i)
		{
			mpp::mesh::Vertex::Component component;
			fread(&component, sizeof(mpp::mesh::Vertex::Component), 1, fp);

			mpp::mesh::Vertex::DataType dataType;
			fread(&dataType, sizeof(mpp::mesh::Vertex::DataType), 1, fp);

			int paddingBytes;
			fread(&paddingBytes, sizeof(int), 1, fp);

			char normalised;
			fread(&normalised, sizeof(char), 1, fp);

			layout->createAttribute(component, dataType, normalised == 1, paddingBytes);
		}
	}

	/*
		* Write all Vertex Buffers.
		*
		*/
	void ModelSerializer::writeVertexBuffers(ofstream& fp)
	{
		uint32_t start = ftell(fp);

		for (size_t i = 0; i < mVertexStreams.size(); ++i)
		{
			writeVertexBuffer(fp, mVertexStreams[i]);
		}

		uint32_t end = ftell(fp);
		updateDirectoryEntry(fp, Directory::Entry::Type::VertexData, start, end, mVertexStreams.size());
	}

	/*
		* Write vertex buffer
		*
		*/
	void ModelSerializer::writeVertexBuffer(ofstream& fp, VertexStream const& vertexStream)
	{
		/*
		4 bytes: vertex data size in bytes
		4 bytes: vertex count
		4 bytes: vertex stride
		x bytes: vertex data
		*/

		size_t vertexDataSize = vertexStream.vertexCount * vertexStream.vertexStride;
		fwrite(&vertexDataSize, sizeof(vertexDataSize), 1, fp);

		fwrite(&vertexStream.vertexCount, sizeof(vertexStream.vertexCount), 1, fp);
		fwrite(&vertexStream.vertexStride, sizeof(vertexStream.vertexStride), 1, fp);
		fwrite(vertexStream.vertexData.get(), vertexDataSize, 1, fp);
	}

	/*
		* Write Vertex Layout
		*
		*/
	void ModelSerializer::writeVertexBufferAttributeLayout(ofstream& fp, mpp::mesh::VertexBufferAttributeLayout const& layout)
	{
		/*
		2 bytes: attribute count

		<attrib count> times:
		4 bytes : component
		4 bytes : data type
		4 bytes : padding bytes
		4 bytes : normalised
		*/

		uint16_t attribCount = layout.getNumAttributes();
		fwrite(&attribCount, sizeof(uint16_t), 1, fp);

		for (int i = 0; i < attribCount; ++i)
		{
			auto attrib = layout.getAttribute(i);

			fwrite(&attrib.component, sizeof(mpp::mesh::Vertex::Component), 1, fp);
			fwrite(&attrib.dataType, sizeof(mpp::mesh::Vertex::DataType), 1, fp);
			fwrite(&attrib.paddingBytes, sizeof(int), 1, fp);

			char normalised = attrib.normalised ? 1 : 0;
			fwrite(&normalised, sizeof(char), 1, fp);
		}
	}

	void ModelSerializer::readIndexBuffers(ifstream& fp)
	{
		auto const& entry = mDirectory.entries[(int)Directory::Entry::Type::IndexData];

		fseek(fp, entry.startOffset, SEEK_SET);

		for (size_t i = 0; i < entry.count; ++i)
		{
			mIndexStreams.push_back(readIndexBuffer(fp));
		}

		if (ftell(fp) != entry.endOffset)
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
		size_t dataSize, indexWidth;

		fread(&dataSize, sizeof(dataSize), 1, fp);
		fread(&indexWidth, sizeof(indexWidth), 1, fp);

		uint8_t* indexData = new uint8_t[dataSize];
		fread(indexData, dataSize, 1, fp);

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
		uint32_t start = ftell(fp);

		for (size_t i = 0; i < mIndexStreams.size(); ++i)
		{
			auto const& mesh = mMeshes[mIndexStreamLookup[i]];
			writeIndexBuffer(fp, mIndexStreams[i], mesh.primitiveType, mesh.primitiveCount);
		}

		uint32_t end = ftell(fp);
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

		fwrite(&indexDataSize, sizeof(indexDataSize), 1, fp);
		fwrite(&indexStream.indexWidth, sizeof(indexStream.indexWidth), 1, fp);
		fwrite(indexStream.indexData.get(), indexDataSize, 1, fp);
	}

	void ModelSerializer::readMeshes(ifstream& fp)
	{
		auto const& entry = mDirectory.entries[(int)Directory::Entry::Type::MeshMetadata];

		fseek(fp, entry.startOffset, SEEK_SET);

		for (size_t i = 0; i < entry.count; ++i)
		{
			mMeshes.push_back(readMesh(fp));
		}

		if (ftell(fp) != entry.endOffset)
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
		4 bytes: material id
		4 bytes: index buffer id (or -1 for none)
		4 bytes: number of vertex buffers
		...    : vertex buffer ids (4 bytes each)
		*/
		Mesh mesh;

		mesh.name = readString(fp);

		fread(&mesh.primitiveType, sizeof(mesh.primitiveType), 1, fp);
		fread(&mesh.primitiveCount, sizeof(mesh.primitiveCount), 1, fp);

		fread(&mesh.material, sizeof(mesh.material), 1, fp);
		fread(&mesh.meshSpec, sizeof(mesh.meshSpec), 1, fp);

		size_t numVertexBuffers;
		fread(&numVertexBuffers, sizeof(numVertexBuffers), 1, fp);

		for (size_t i = 0; i < numVertexBuffers; ++i)
		{
			uint32_t vertexBufferId;
			fread(&vertexBufferId, sizeof(vertexBufferId), 1, fp);

			mesh.vertexStreams.push_back(vertexBufferId);
		}

		fread(&mesh.indexStream, sizeof(mesh.indexStream), 1, fp);

		return mesh;
	}

	void ModelSerializer::writeMeshes(ofstream& fp)
	{
		uint32_t start = ftell(fp);

		for (uint32_t i = 0; i < mMeshes.size(); ++i)
		{
			writeMesh(fp, mMeshes[i], i);
		}

		uint32_t end = ftell(fp);
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
		writeString(fp, mesh.name);
		fwrite(&mesh.primitiveType, sizeof(mesh.primitiveType), 1, fp);
		fwrite(&mesh.primitiveCount, sizeof(mesh.primitiveCount), 1, fp);

		// Write material, meshspec, index buffer and vertex buffer IDs
		fwrite(&mesh.material, sizeof(mesh.material), 1, fp);
		fwrite(&mesh.meshSpec, sizeof(index), 1, fp);

		auto numVertexBuffers = mesh.vertexStreams.size();
		fwrite(&numVertexBuffers, sizeof(numVertexBuffers), 1, fp);
		for (auto vb: mesh.vertexStreams)
		{
			fwrite(&vb, sizeof(vb), 1, fp);
		}

		fwrite(&mesh.indexStream, sizeof(uint32_t), 1, fp);
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

	void ModelSerializer::addMaterial(string const& name, MaterialInformation const& matInfo)
	{
		mMaterialLookup[name] = mMaterials.size();
		mMaterials.push_back(matInfo);
	}

	/*
		* Set material.
		*
		*/
	void ModelSerializer::setMaterial(size_t meshIndex, string const& material)
	{
		mMeshes[meshIndex].material = mMaterialLookup[material];
	}

	/*
		* Get material.
		*
		*/
	string const& ModelSerializer::getMaterial(size_t meshIndex) const
	{
		return mMaterials[mMeshes[meshIndex].material].getName();
	}

	vector<MaterialInformation> const& ModelSerializer::getMaterials() const
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
#pragma warning(suppress: 4996)
		FILE* fp = fopen(filename.c_str(), "wb");
		
		writeHeader(fp);
		writeDirectory(fp);

		writeMaterials(fp);
		writeMeshSpecifications(fp);
		writeVertexBuffers(fp);
		writeIndexBuffers(fp);
		writeMeshes(fp);
			
		fclose(fp);
	}

	/*
		* Load file.
		*
		*/
	void ModelSerializer::load(std::string const& filename)
	{
		clear();

#pragma warning(suppress: 4996)
		FILE* fp = fopen(filename.c_str(), "rb");

		readHeader(fp);
		readDirectory(fp);

		readMaterials(fp);
		readMeshSpecifications(fp);
		readVertexBuffers(fp);
		readIndexBuffers(fp);
		readMeshes(fp);

		fclose(fp);
	}

	ModelSerializer::MetadataReader ModelSerializer::getReader(string const& filename)
	{
		// If we pass in a filename, then load it in.  Otherwise, assume the
		// data has already been loaded (either from a file, or specified from API).
		if (filename != "")
		{
			clear();

#pragma warning(suppress: 4996)
			FILE* fp = fopen(filename.c_str(), "rb");

			readHeader(fp);
			readDirectory(fp);

			readMaterials(fp);
			readMeshSpecifications(fp);
			readMeshes(fp);

			fclose(fp);
		}

		MetadataReader reader(
			mMaterialLookup,
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


