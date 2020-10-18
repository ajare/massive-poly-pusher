#include "mpp/mesh/ModelSerializer.h"
#include "mpp/mesh/MppMeshException.h"

#define FLAG_INDEXED_VERTICES 0x0001

using namespace std;

namespace mpp
{
	namespace mesh
	{

		/*
		 * Constructor.
		 *
		 */
		ModelSerializer::ModelSerializer()
			: mMaterialPeakOffset(0)
			, mMeshSpecPeakOffset(0)
		{
		}

		string ModelSerializer::readString(FILE* fp)
		{
			string ret;
			auto ch = fgetc(fp);
			while (ch != '\0')
			{
				ret.append(1, ch);
				ch = fgetc(fp);
			}

			return ret;
		}

		void ModelSerializer::writeString(FILE* fp, string const& str)
		{
			auto len = str.length();
			fwrite(str.c_str(), 1, str.length() + 1, fp);
		}

		/*
		 * Read Header
		 *
		 */
		void ModelSerializer::readHeader(FILE* fp)
		{
			/*
			4 bytes: id: value 'MMPM'
			2 bytes: version major
			2 bytes: version minor
			4 bytes: flags (including if indexed, etc).
			*/

			// Id
			char magic[4];
			fread(magic, 4, 1, fp);

			if (magic[0] != 'M' || magic[1] != 'P' || magic[2] != 'P' || magic[3] != 'M')
			{
				throw MppMeshException("File is not a valid model file.");
			}

			// Version major
			uint16 versionMajor;
			fread(&versionMajor, sizeof(uint16), 1, fp);

			mHeader.versionMajor = versionMajor;

			// Version minor
			uint16 versionMinor;
			fread(&versionMinor, sizeof(uint16), 1, fp);

			mHeader.versionMinor = versionMinor;

			// Flags
			fread(&mHeader.flags, sizeof(uint32), 1, fp);

			// Peak info
			fread(&mMaterialPeakOffset, sizeof(uint32), 1, fp);
			fread(&mMeshSpecPeakOffset, sizeof(uint32), 1, fp);
		}

		void ModelSerializer::skipHeader(FILE* fp)
		{
			auto offset = 4 + sizeof(uint16) + sizeof(uint16) + sizeof(uint32);
			fseek(fp, offset, SEEK_SET);
		}

		/*
		 * Write Header
		  *
		*/
		void ModelSerializer::writeHeader(FILE* fp)
		{
			/*
			4 bytes: id: value 'MMPM'
			2 bytes: version major
			2 bytes: version minor
			4 bytes: flags (including if indexed, etc).
			4 bytes: material peak info
			4 bytes: meshspec peak info
			*/

			// Id
			char magic[4] = { 'M', 'P', 'P', 'M' };
			fwrite(magic, 4, 1, fp);

			// Version major
			uint16 versionMajor = 1;
			fwrite(&versionMajor, sizeof(uint16), 1, fp);

			// Version minor
			uint16 versionMinor = 1;
			fwrite(&versionMinor, sizeof(uint16), 1, fp);

			// Flags
			uint32 flags = 0;
			flags |= FLAG_INDEXED_VERTICES;

			fwrite(&flags, sizeof(uint32), 1, fp);

			// Peak info
			uint32 offsetDummy{ 0 };

			mMaterialPeakOffset = ftell(fp);
			fwrite(&offsetDummy, sizeof(offsetDummy), 1, fp);

			mMeshSpecPeakOffset = ftell(fp);
			fwrite(&offsetDummy, sizeof(offsetDummy), 1, fp);
		}

		/*
		 * Read Vertex Layout
		 *
		 */
		void ModelSerializer::readVertexBufferAttributeLayout(FILE* fp, mpp::mesh::VertexBufferAttributeLayout* layout, uint32_t attribOffset)
		{
			/*
			2 bytes: attribute count

			<attrib count> times:
			4 bytes : component
			4 bytes : data type
			4 bytes : padding bytes
			4 bytes : normalised
			*/

			uint16 attribCount;
			fread(&attribCount, sizeof(uint16), 1, fp);

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
		 * Write Vertex Layout
		 *
		 */
		void ModelSerializer::writeVertexBufferAttributeLayout(FILE* fp, mpp::mesh::VertexBufferAttributeLayout const& layout)
		{
			/*
			2 bytes: attribute count

			<attrib count> times:
			4 bytes : component
			4 bytes : data type
			4 bytes : padding bytes
			4 bytes : normalised
			*/

			uint16 attribCount = layout.getNumAttributes();
			fwrite(&attribCount, sizeof(uint16), 1, fp);

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

		/*
		 * Read model specification
		 *
		 */
		MeshSpecification ModelSerializer::readMeshSpecification(FILE* fp, string& meshName)
		{
			/*
			x bytes: mesh name
			4 bytes: primitive type
			4 bytes: storage type
			1 byte : indexed vertices?
			2 bytes: layout count
			<buffer count> times: buffer data
			*/

			meshName = readString(fp);

			MeshSpecification meshSpec;

			mpp::mesh::Primitive::Type primitiveType;
			fread(&primitiveType, sizeof(mpp::mesh::Primitive::Type), 1, fp);
			meshSpec.setPrimitiveType(primitiveType);

			mpp::mesh::VertexBufferStorageType storageType;
			fread(&storageType, sizeof(mpp::mesh::VertexBufferStorageType), 1, fp);
			meshSpec.setStorageType(storageType);

			char indexed;
			fread(&indexed, sizeof(char), 1, fp);
			if (indexed == 1)
			{
				meshSpec.setIndexedVertices(true);
			}

			uint16 layoutCount;
			fread(&layoutCount, sizeof(uint16), 1, fp);

			uint32_t attribOffset = 0;
			for (int i = 0; i < layoutCount; ++i)
			{
				auto layout = meshSpec.createVertexBufferAttributeLayout(false);
				readVertexBufferAttributeLayout(fp, layout, attribOffset);
				attribOffset += layout->getNumAttributes();
			}

			return meshSpec;
		}


		/*
		 * Write model specification
		 *
		 */
		void ModelSerializer::writeMeshSpecification(FILE* fp, string const&meshName, MeshSpecification const& meshSpec)
		{
			/*
			x bytes: mesh name
			4 bytes: primitive type
			4 bytes: storage type
			1 byte : indexed vertices?
			2 bytes: layout count
			<buffer count> times: buffer data
			*/

			writeString(fp, meshName);

			mpp::mesh::Primitive::Type primitiveType = meshSpec.getPrimitiveType();
			fwrite(&primitiveType, sizeof(mpp::mesh::Primitive::Type), 1, fp);

			mpp::mesh::VertexBufferStorageType storageType = meshSpec.getStorageType();
			fwrite(&storageType, sizeof(mpp::mesh::VertexBufferStorageType), 1, fp);

			char indexed = meshSpec.verticesIndexed() ? 1 : 0;
			fwrite(&indexed, sizeof(char), 1, fp);

			uint16 layoutCount = meshSpec.getNumVertexBufferAttributeLayouts();
			fwrite(&layoutCount, sizeof(uint16), 1, fp);

			for (int i = 0; i < meshSpec.getNumVertexBufferAttributeLayouts(); ++i)
			{
				auto buffer = meshSpec.getVertexBufferAttributeLayout(i);
				writeVertexBufferAttributeLayout(fp, buffer);
			}
		}

		/*
		 * Read vertex buffer
		 *
		 */
		void ModelSerializer::readVertexBuffer(FILE* fp, int meshIndex)
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

			int8* vertexData = new int8[vertexDataSize];
			fread(vertexData, vertexDataSize, 1, fp);

			VertexStream vs;
			vs.vertexCount = vertexCount;
			vs.vertexStride = vertexStride;
			vs.vertexData = shared_ptr<const int8>(vertexData, [](int8 *p) { delete[] p; });

			mMeshes[meshIndex].vertexStreams.push_back(vs);
		}

		/*
		 * Write vertex buffer
		 *
		 */
		void ModelSerializer::writeVertexBuffer(FILE* fp, VertexStream const& vertexStream)
		{
			/*
			4 bytes: vertex data size in bytes
			4 bytes: vertex count
			4 bytes: vertex stride
			x bytes: vertex data
			*/

			int vertexDataSize = vertexStream.vertexCount * vertexStream.vertexStride;
			fwrite(&vertexDataSize, sizeof(int), 1, fp);

			fwrite(&vertexStream.vertexCount, sizeof(int), 1, fp);
			fwrite(&vertexStream.vertexStride, sizeof(int), 1, fp);
			fwrite(vertexStream.vertexData.get(), vertexDataSize, 1, fp);
		}

		MaterialInformation ModelSerializer::readMaterialInformation(FILE* fp)
		{
			auto name = readString(fp);

			MaterialInformation mi(name);

			mpp::mesh::MaterialInformation::PositionType positionType;
			fread(&positionType, sizeof(positionType), 1, fp);

			mi.setPositionType(positionType);

			int shaderCount;
			fread(&shaderCount, sizeof(shaderCount), 1, fp);
			for (int i = 0; i < shaderCount; ++i)
			{
				mpp::mesh::MaterialInformation::Shader::Type shaderType;

				fread(&shaderType, sizeof(shaderType), 1, fp);
				auto shaderName = readString(fp);

				mi.addShader(shaderType, shaderName);
			}

			int textureCount;
			fread(&textureCount, sizeof(textureCount), 1, fp);
			for (int i = 0; i < textureCount; ++i)
			{
				int32 isResource;
				fread(&isResource, sizeof(isResource), 1, fp);
				auto binding = readString(fp);
				auto resource = readString(fp);

				mi.addTexture(isResource, binding, resource);
			}

			return mi;
		}

		void ModelSerializer::writeMaterialInformation(FILE* fp, MaterialInformation const& matInfo)
		{
			writeString(fp, matInfo.getName());

			auto positionType = matInfo.getPositionType();
			fwrite(&positionType, sizeof(positionType), 1, fp);

			auto const& shaders = matInfo.getShaders();

			int shaderCount = (int)shaders.size();
			fwrite(&shaderCount, sizeof(shaderCount), 1, fp);
			for (auto const& shader: shaders)
			{
				fwrite(&shader.type, sizeof(shader.type), 1, fp);
				writeString(fp, shader.name);
			}

			auto const& textures = matInfo.getTextures();

			int textureCount = (int)textures.size();
			fwrite(&textureCount, sizeof(textureCount), 1, fp);
			for (auto const& texture : textures)
			{
				int32 isResource = texture.isResource ? 1 : 0;
				fwrite(&isResource, sizeof(isResource), 1, fp);
				writeString(fp, texture.binding);
				writeString(fp, texture.resource);
			}
		}

		/*
		 * Read mesh definition
		 *
		 */
		void ModelSerializer::readMeshDefinition(FILE* fp, int meshIndex)
		{
			/*
			4 bytes: primitive count
			zero-str: name
			zero-str: material name
			4 bytes: index width
			4 bytes: index data size in bytes
			x bytes: index data
			*/

			// Read primitive type
			Primitive::Type primitiveType;
			fread(&primitiveType, sizeof(Primitive::Type), 1, fp);
			mMeshes[meshIndex].primitiveType = primitiveType;

			// Read primitive count
			int primitiveCount;
			fread(&primitiveCount, sizeof(int), 1, fp);
			mMeshes[meshIndex].primitiveCount = primitiveCount;

			// Read name
			string name;
			int c = fgetc(fp);
			while (c != 0)
			{
				name.push_back((char)c);
				c = fgetc(fp);
			}

			mMeshes[meshIndex].name = name;

			// Read material
			string materialName;
			c = fgetc(fp);
			while (c != 0)
			{
				materialName.push_back((char)c);
				c = fgetc(fp);
			}

			mMeshes[meshIndex].material = materialName;

			// Read index data
			int indexWidth;
			fread(&indexWidth, sizeof(int), 1, fp);

			mMeshes[meshIndex].indexWidth = indexWidth;

			int indexDataSize;
			fread(&indexDataSize, sizeof(int), 1, fp);

			uint8* indexData = nullptr;
			if (indexDataSize > 0)
			{
				indexData = new uint8[3 * primitiveCount * (indexWidth / 8)];
				fread(indexData, indexDataSize, 1, fp);
			}

			mMeshes[meshIndex].indexData.reset(indexData);

			// Read vertex buffers
			for (int i = 0; i < mMeshes[meshIndex].specification.getNumVertexBufferAttributeLayouts(); ++i)
			{
				readVertexBuffer(fp, meshIndex);
			}
		}

		/*
		 * Write mesh definition
		 *
		 */
		void ModelSerializer::writeMeshDefinition(FILE* fp, int meshIndex)
		{
			/*
			4 bytes: primitive count
			zero-str: name
			zero-str: material name
			4 bytes: index width
			4 bytes: index data size in bytes
			x bytes: index data
			*/

			// Write primitive type
			fwrite(&mMeshes[meshIndex].primitiveType, sizeof(Primitive::Type), 1, fp);

			// Write primitive count
			fwrite(&mMeshes[meshIndex].primitiveCount, sizeof(int), 1, fp);

			// Write name
			fputs(mMeshes[meshIndex].name.c_str(), fp);
			fputc(0, fp);

			// Write material
			fputs(mMeshes[meshIndex].material.c_str(), fp);
			fputc(0, fp);

			// Write index data
			int indexWidth = mMeshes[meshIndex].indexWidth;

			fwrite(&indexWidth, sizeof(int), 1, fp);
			auto indexData = mMeshes[meshIndex].indexData.get();
			int indexDataSize = indexData != nullptr ? mMeshes[meshIndex].primitiveCount * mesh::Primitive::size(mMeshes[meshIndex].primitiveType) * (indexWidth / 8) : 0;
			fwrite(&indexDataSize, sizeof(int), 1, fp);

			fwrite(indexData, indexDataSize, 1, fp);

			for (auto vertexStream: mMeshes[meshIndex].vertexStreams)
			{
				writeVertexBuffer(fp, vertexStream);
			}
		}

		/*
		 * Set mesh specification.
		 *
		 */
		void ModelSerializer::setMeshSpecification(int meshIndex, MeshSpecification const& specification)
		{
			mMeshes[meshIndex].specification = specification;
		}

		/*
		 * Get mesh specification.
		 *
		 */
		MeshSpecification const& ModelSerializer::getMeshSpecification(int meshIndex) const
		{
			return mMeshes[meshIndex].specification;
		}

		map<string, MeshSpecification> ModelSerializer::peakMeshSpecification(string const& filename)
		{
#pragma warning(suppress: 4996)
			FILE* fp = fopen(filename.c_str(), "rb");

			readHeader(fp);
			fseek(fp, mMeshSpecPeakOffset, SEEK_SET);

			map<string, MeshSpecification> meshSpecs;

			uint32 meshCount;
			fread(&meshCount, sizeof(meshCount), 1, fp);

			for (uint32 i = 0; i < mMeshes.size(); ++i)
			{
				string meshName;
				auto meshSpec = readMeshSpecification(fp, meshName);
				meshSpecs[meshName] = meshSpec;
			}

			fclose(fp);

			return meshSpecs;
		}

		void ModelSerializer::addMaterialInformation(string const& name, MaterialInformation const& matInfo)
		{
			mMaterialInformation[name] = matInfo;
		}

		map<string, MaterialInformation> const& ModelSerializer::getMaterialInformation() const
		{
			return mMaterialInformation;
		}

		map<string, MaterialInformation> ModelSerializer::peakMaterialInformation(string const& filename)
		{
#pragma warning(suppress: 4996)
			FILE* fp = fopen(filename.c_str(), "rb");

			readHeader(fp);
			fseek(fp, mMaterialPeakOffset, SEEK_SET);

			// Read materials
			int materialCount;
			fread(&materialCount, sizeof(int), 1, fp);

			map<string, MaterialInformation> mats;
			for (int i = 0; i < materialCount; ++i)
			{
				auto mi = readMaterialInformation(fp);
				mats[mi.getName()] = mi;
			}

			fclose(fp);

			return mats;
		}

		/*
		 * Set name.
		 *
		 */
		void ModelSerializer::setName(int meshIndex, string const& name)
		{
			mMeshes[meshIndex].name = name;
		}

		/*
		 * Get name.
		 *
		 */
		string const& ModelSerializer::getName(int meshIndex) const
		{
			return mMeshes[meshIndex].name;
		}

		/*
		 * Set material.
		 *
		 */
		void ModelSerializer::setMaterial(int meshIndex, string const& material)
		{
			mMeshes[meshIndex].material = material;
		}

		/*
		 * Get material.
		 *
		 */
		string const& ModelSerializer::getMaterial(int meshIndex) const
		{
			return mMeshes[meshIndex].material;
		}

		void ModelSerializer::setIndexWidth(int meshIndex, int width)
		{
			mMeshes[meshIndex].indexWidth = width;
		}

		int ModelSerializer::getIndexWidth(int meshIndex) const
		{
			return mMeshes[meshIndex].indexWidth;
		}

		/*
		 * Set index data.
		 *
		 */
		void ModelSerializer::setIndexData(int meshIndex, shared_ptr<const uint8> indexData)
		{
			mMeshes[meshIndex].indexData = indexData;
		}

		/*
		 * Get index data.
		 *
		 */
		shared_ptr<const uint8> ModelSerializer::getIndexData(int meshIndex) const
		{
			return mMeshes[meshIndex].indexData;
		}

		/*
		 * Set primitive type.
		 *
		 */
		void ModelSerializer::setPrimitiveType(int meshIndex, Primitive::Type primitiveType)
		{
			mMeshes[meshIndex].primitiveType = primitiveType;
		}

		/*
		 * Get primitive type.
		 *
		 */
		Primitive::Type ModelSerializer::getPrimitiveType(int meshIndex) const
		{
			return mMeshes[meshIndex].primitiveType;
		}

		/*
		 * Set primitive count.
		 *
		 */
		void ModelSerializer::setPrimitiveCount(int meshIndex, int primitiveCount)
		{
			mMeshes[meshIndex].primitiveCount = primitiveCount;
		}

		/*
		 * Get primitive count.
		 *
		 */
		int ModelSerializer::getPrimitiveCount(int meshIndex) const
		{
			return mMeshes[meshIndex].primitiveCount;
		}

		/*
		 * Add a vertex stream.
		 *
		 */
		void ModelSerializer::addVertexStream(int meshIndex, int vertexCount, int vertexStride, std::shared_ptr<const int8> vertexData)
		{
			VertexStream vs;
			vs.vertexCount = vertexCount;
			vs.vertexStride = vertexStride;
			vs.vertexData = vertexData;

			mMeshes[meshIndex].vertexStreams.push_back(vs);
		}

		/*
		 * Get specified vertex stream.
		 *
		 */
		void ModelSerializer::getVertexStream(int meshIndex, int index, int* vertexCount, int* vertexStride, shared_ptr<const int8>* vertexData)
		{
			*vertexCount = mMeshes[meshIndex].vertexStreams[index].vertexCount;
			*vertexStride = mMeshes[meshIndex].vertexStreams[index].vertexStride;
			*vertexData = mMeshes[meshIndex].vertexStreams[index].vertexData;
		}

		/*
		 * Set the number of meshes in this file.
		 *
		 */
		void ModelSerializer::setMeshCount(int count)
		{
			mMeshes.resize(count);
		}

		/*
		 * Get the number of meshes in this file.
		 *
		 */
		int ModelSerializer::getMeshCount() const
		{
			return (int)mMeshes.size();
		}

		/*
		 * Save file.
		 *
		 */
		void ModelSerializer::save(string const& filename)
		{
#pragma warning(suppress: 4996)
			FILE* fp = fopen(filename.c_str(), "wb");

			// Header with peak info
			mMaterialPeakOffset = 0;
			mMeshSpecPeakOffset = 0;
			writeHeader(fp);

			// Write materials
			auto curOffset = ftell(fp);
			fseek(fp, mMaterialPeakOffset, SEEK_SET);
			fwrite(&curOffset, sizeof(curOffset), 1, fp);
			fseek(fp, curOffset, SEEK_SET);

			int materialCount = (int)mMaterialInformation.size();
			fwrite(&materialCount, sizeof(int), 1, fp);

			for (auto const& matInfo: mMaterialInformation)
			{
				writeMaterialInformation(fp, matInfo.second);
			}

			// Write meshspecs
			curOffset = ftell(fp);
			fseek(fp, mMeshSpecPeakOffset, SEEK_SET);
			fwrite(&curOffset, sizeof(curOffset), 1, fp);
			fseek(fp, curOffset, SEEK_SET);

			int meshCount = getMeshCount();

			fwrite(&meshCount, sizeof(int), 1, fp);
			for (uint32 i = 0; i < mMeshes.size(); ++i)
			{
				writeMeshSpecification(fp, mMeshes[i].name, mMeshes[i].specification);
			}

			// Write meshes
			fwrite(&meshCount, sizeof(int), 1, fp);
			for (uint32 i = 0; i < mMeshes.size(); ++i)
			{
				writeMeshDefinition(fp, i);
			}

			fclose(fp);
		}

		/*
		 * Load file.
		 *
		 */
		void ModelSerializer::load(std::string const& filename)
		{
#pragma warning(suppress: 4996)
			FILE* fp = fopen(filename.c_str(), "rb");

			readHeader(fp);

			// Read materials
			int materialCount;
			fread(&materialCount, sizeof(materialCount), 1, fp);

			for (int i = 0; i < materialCount; ++i)
			{
				auto mi = readMaterialInformation(fp);
				mMaterialInformation[mi.getName()] = mi;
			}

			// Read meshspecs: assume 1-to-1 spec to mesh.
			int meshCount;
			fread(&meshCount, sizeof(meshCount), 1, fp);
			setMeshCount(meshCount);

			for (uint32 i = 0; i < mMeshes.size(); ++i)
			{
				string meshName;
				mMeshes[i].specification = readMeshSpecification(fp, meshName);
			}

			// Read meshes
			fread(&meshCount, sizeof(meshCount), 1, fp);
			for (uint32 i = 0; i < mMeshes.size(); ++i)
			{
				readMeshDefinition(fp, i);
			}

			fclose(fp);
		}
	}
}

