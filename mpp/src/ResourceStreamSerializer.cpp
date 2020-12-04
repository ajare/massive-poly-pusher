#include "mpp/program/ShaderStage.h"

#include "mpp/ResourceStreamSerializer.h"
#include "mpp/MaterialStream.h"
#include "mpp/ProgramStream.h"
#include "mpp/SamplerStream.h"
#include "mpp/StringStream.h"
#include "mpp/TextureStream.h"
#include "mpp/ProgrammaticMaterialStream.h"
#include "mpp/ProgrammaticProgramStream.h"
#include "mpp/ProgrammaticSamplerStream.h"
#include "mpp/ProgrammaticStringStream.h"
#include "mpp/ProgrammaticTextureStream.h"
#include "mpp/MppException.h"

namespace mpp
{
	using namespace std;

	ResourceStreamSerializer::ResourceStreamSerializer(ResourceManager* resourceMgr)
		: mResourceMgr(resourceMgr)
	{
	}

	void ResourceStreamSerializer::setGlobalMeshSpecification(mesh::MeshSpecification const& meshSpec)
	{
		mUseGlobalMeshSpec = true;
		mMeshSpec = meshSpec;
	}

	void ResourceStreamSerializer::serialize(ResourceStreamPtr resourceStream, string const& filename)
	{
		// Open file
		ofstream fp(filename, ios::out | ios::binary);

		if (!fp.is_open())
		{
			THROW_MPP_IO("Could not open " + filename + " for writing", __LINE__, __FILE__, __func__);
		}

		serialize(resourceStream, fp);

		fp.close();
	}

	void ResourceStreamSerializer::serialize(ResourceStreamPtr resourceStream, ofstream& fp)
	{
		// Write magic number
		char* magic{ "RSER" };
		fp.write(magic, 4);

		// Recursively write streams and their children
		writeStream(resourceStream, fp);
	}

	ResourceStreamPtr ResourceStreamSerializer::deserialize(string const& filename)
	{
		// Open file
		ifstream fp(filename, ios::in | ios::binary);

		if (!fp.is_open())
		{
			THROW_MPP_IO("Could not open " + filename + " for reading", __LINE__, __FILE__, __func__);
		}

		return deserialize(fp);
	}

	ResourceStreamPtr ResourceStreamSerializer::deserialize(ifstream& fp)
	{
		char magic[4];
		fp.read(magic, 4);

		if (magic[0] != 'R' && magic[1] != 'S' && magic[2] != 'E' && magic[3] != 'R')
		{
			THROW_MPP_IO("Could not open stream for reading.  Not a valid format.", __LINE__, __FILE__, __func__);
		}

		return readStream(fp);
	}

	void ResourceStreamSerializer::writeValue(string const& value, ofstream& fp)
	{
		size_t len = value.length();
		fp.write((char const*)&len, sizeof(len));

		if (len > 0)
		{
			fp.write(value.c_str(), len);
		}
	}

	void ResourceStreamSerializer::writeValue(int32_t value, ofstream& fp)
	{
		fp.write((char const*)&value, sizeof(value));
	}

	void ResourceStreamSerializer::writeValue(uint32_t value, ofstream& fp)
	{
		fp.write((char const*)&value, sizeof(value));
	}

	void ResourceStreamSerializer::writeValue(float value, ofstream& fp)
	{
		fp.write((char const*)&value, sizeof(value));
	}

	void ResourceStreamSerializer::writeValue(bool value, ofstream& fp)
	{
		char vi = value ? 1 : 0;
		fp.write((char const*)&vi, sizeof(vi));
	}

	void ResourceStreamSerializer::writeMeshSpecification(mesh::MeshSpecification const& meshSpec, ofstream& fp)
	{
		auto const& ms = mUseGlobalMeshSpec ? mMeshSpec : meshSpec;

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

	void ResourceStreamSerializer::writeUniformCollection(UniformCollection const& uniforms, ofstream& fp)
	{
		writeValue(uniforms.getNumUniforms(), fp);

		auto const& uniformData = uniforms.getUniformData();

		for (auto const& kvp: uniformData)
		{
			auto const& data = kvp.second;

			writeValue(data.name, fp);
			writeValue((uint32_t)data.type, fp);
			writeValue(data.size, fp);

			fp.write(data.data, 64);
		}
	}

	void ResourceStreamSerializer::writeParser(program::Parser const& parser, ofstream& fp)
	{
		writeValue(parser.getName(), fp);

		// Shader stages
		for (uint32_t i = 0; i < (uint32_t)program::ShaderStage::Type::NumStages; ++i)
		{
			auto const& stage = parser.getStage(i);

			writeValue(stage.inputSource, fp);
		}

		writeMeshSpecification(parser.getMeshSpecification(), fp);
	}

	void ResourceStreamSerializer::writeMaterialStream(ResourceStreamPtr resourceStream, ofstream& fp)
	{
		auto stream = dynamic_cast<MaterialStream*>(resourceStream.get());

		// Write number of quality settings
		writeValue(stream->mQualitySettings.size(), fp);

		// Write quality settings
		for (auto const& setting: stream->mQualitySettings)
		{
			// Program options
			writeValue(setting.spec.program.resourceExists, fp);
			writeValue(setting.spec.program.existingResource, fp);
			writeValue(setting.spec.program.isChild, fp);
			writeValue(setting.spec.program.is2d, fp);

			writeMeshSpecification(setting.spec.program.spec, fp);

			// Shaders
			writeValue((uint32_t)setting.spec.program.vertexShader.type, fp);
			writeValue(setting.spec.program.vertexShader.data, fp);

			writeValue((uint32_t)setting.spec.program.geometryShader.type, fp);
			writeValue(setting.spec.program.geometryShader.data, fp);

			writeValue((uint32_t)setting.spec.program.fragmentShader.type, fp);
			writeValue(setting.spec.program.fragmentShader.data, fp);

			// Uniforms
			writeUniformCollection(setting.spec.uniforms, fp);

			// Textures
			writeValue(setting.spec.textures.size(), fp);
			for (auto const& kvp: setting.spec.textures)
			{
				writeValue(kvp.first, fp);
				writeValue(kvp.second.first, fp);
				writeValue(kvp.second.second, fp);
			}
		}
	}

	void ResourceStreamSerializer::writeProgramStream(ResourceStreamPtr resourceStream, ofstream& fp)
	{
		auto stream = dynamic_cast<ProgramStream*>(resourceStream.get());

		// Write source
		writeValue(stream->mVertexSource, fp);
		writeValue(stream->mGeometrySource, fp);
		writeValue(stream->mFragmentSource, fp);

		// Write number of quality settings
		writeValue(stream->mQualitySettings.size(), fp);

		// Write quality settings
		for (auto const& setting: stream->mQualitySettings)
		{
			writeParser(*setting.parser.get(), fp);

			// Vertex shader
			writeValue((uint32_t)setting.vertexShader.type, fp);
			writeValue(setting.vertexShader.source, fp);
			writeValue(setting.vertexShader.data, fp);

			// Geometry shader
			writeValue((uint32_t)setting.geometryShader.type, fp);
			writeValue(setting.geometryShader.source, fp);
			writeValue(setting.geometryShader.data, fp);

			// Fragment shader
			writeValue((uint32_t)setting.fragmentShader.type, fp);
			writeValue(setting.fragmentShader.source, fp);
			writeValue(setting.fragmentShader.data, fp);
		}

		// Write attributes
		writeValue(stream->mAttribs.size(), fp);
		for (auto const& attrib: stream->mAttribs)
		{
			writeValue(attrib, fp);
		}
	}

	void ResourceStreamSerializer::writeSamplerStream(ResourceStreamPtr resourceStream, ofstream& fp)
	{
		auto stream = dynamic_cast<SamplerStream*>(resourceStream.get());

		// Write number of quality settings
		writeValue(stream->mQualitySettings.size(), fp);

		// Write quality settings
		for (auto const& setting: stream->mQualitySettings)
		{
			writeValue(setting.params.minFilter, fp);
			writeValue(setting.params.magFilter, fp);
			writeValue(setting.params.wrap, fp);
			writeValue(setting.params.lodMinLevel, fp);
			writeValue(setting.params.lodMaxLevel, fp);
			writeValue(setting.params.lodBias, fp);
			writeValue(setting.params.maxAnisotropy, fp);
		}
	}

	void ResourceStreamSerializer::writeStringStream(ResourceStreamPtr resourceStream, ofstream& fp)
	{
		auto stream = dynamic_cast<StringStream*>(resourceStream.get());

		// Write number of quality settings
		writeValue(stream->mQualitySettings.size(), fp);

		// Write quality settings
		for (auto const& setting: stream->mQualitySettings)
		{
			writeValue(setting.data, fp);
			writeValue(setting.file, fp);
			writeValue(setting.isFile, fp);
		}
	}

	void ResourceStreamSerializer::writeTextureStream(ResourceStreamPtr resourceStream, ofstream& fp)
	{
		auto stream = dynamic_cast<TextureStream*>(resourceStream.get());

		// Write tiles
		writeValue(stream->mTiles.size(), fp);
		for (auto const& kvp: stream->mTiles)
		{
			writeValue(kvp.first, fp);
			writeValue(kvp.second.u[0], fp);
			writeValue(kvp.second.u[1], fp);
			writeValue(kvp.second.v[0], fp);
			writeValue(kvp.second.v[1], fp);
		}

		// Write number of quality settings
		writeValue(stream->mQualitySettings.size(), fp);

		// Write quality settings
		for (auto const& setting: stream->mQualitySettings)
		{
			// Texture params
			writeValue(setting.params.minFilter, fp);
			writeValue(setting.params.magFilter, fp);
			writeValue(setting.params.wrap, fp);
			writeValue(setting.params.useMipmaps, fp);
			writeValue(setting.params.lodBaseLevel, fp);
			writeValue(setting.params.lodMaxLevel, fp);
			writeValue(setting.params.lodBias, fp);
			writeValue(setting.params.maxAnisotropy, fp);

			// Other.  Don't write the image load function, this will be provided by whatever loads this.
			writeValue(setting.sampler, fp);
			writeValue(setting.source, fp);
		}

		writeValue(stream->mInternalFormat, fp);
		writeValue(stream->mTarget, fp);
	}

	void ResourceStreamSerializer::writeStream(ResourceStreamPtr resourceStream, ofstream& fp)
	{
		// Write type
		auto const& streamType = resourceStream->getType();
		writeValue(streamType, fp);

		// Write children
		auto const& children = resourceStream->getChildren();

		writeValue(children.size(), fp);
		for (auto const& kvp: children)
		{
			auto const& name = kvp.first;
			auto const& child = kvp.second;

			writeValue(name, fp);
			writeStream(child, fp);
		}

		// Write number of quality settings
		writeValue(resourceStream->mQualityNames.size(), fp);

		// Write quality settings names
		for (auto const& kvp: resourceStream->mQualityNames)
		{
			auto const& name = kvp.first;
			auto id = kvp.second;

			writeValue(name, fp);
			writeValue(id, fp);
		}

		// Write type-specific data
		if (streamType == "Material")
		{
			writeMaterialStream(resourceStream, fp);
		}
		else if (streamType == "Program")
		{
			writeProgramStream(resourceStream, fp);
		}
		else if (streamType == "Sampler")
		{
			writeSamplerStream(resourceStream, fp);
		}
		else if (streamType == "String")
		{
			writeStringStream(resourceStream, fp);
		}
		else if (streamType == "Texture")
		{
			writeTextureStream(resourceStream, fp);
		}
		else
		{
			string errMsg = "Cannot serialize ResourceStream of type '" + streamType + "'.";
			THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
		}
	}

	string ResourceStreamSerializer::readString(ifstream& fp)
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

	int32_t ResourceStreamSerializer::readInt(ifstream& fp)
	{
		int32_t value;

		fp.read((char*)&value, sizeof(value));
		return value;
	}

	uint32_t ResourceStreamSerializer::readUInt(ifstream& fp)
	{
		uint32_t value;

		fp.read((char*)&value, sizeof(value));
		return value;
	}

	float ResourceStreamSerializer::readFloat(ifstream& fp)
	{
		float value;

		fp.read((char*)&value, sizeof(value));
		return value;
	}

	bool ResourceStreamSerializer::readBool(ifstream& fp)
	{
		char value;

		fp.read((char*)&value, sizeof(value));
		return value != 0;
	}

	mesh::MeshSpecification ResourceStreamSerializer::readMeshSpecification(ifstream& fp)
	{
		mesh::MeshSpecification meshSpec;

		auto primitiveType = static_cast<mesh::Primitive::Type>(readUInt(fp));
		meshSpec.setPrimitiveType(primitiveType);

		auto storageType = static_cast<mesh::VertexBufferStorageType>(readUInt(fp));
		meshSpec.setStorageType(storageType);

		auto indexed = readBool(fp);
		meshSpec.setIndexedVertices(indexed);

		size_t numLayouts = readUInt(fp);
		for (size_t i = 0; i < numLayouts; ++i)
		{
			auto isStatic = readBool(fp);

			auto layout = meshSpec.createVertexBufferAttributeLayout(isStatic);

			size_t numAttribs = readUInt(fp);
			for (size_t j = 0; j < numAttribs; ++j)
			{
				auto component = static_cast<mesh::Vertex::Component>(readUInt(fp));
				auto datatype = static_cast<mesh::Vertex::DataType>(readUInt(fp));
				auto padToBoundary = readInt(fp);
				auto normalised = readBool(fp);

				// Need to sort out paddingBytes
				layout->createAttribute(component, datatype, normalised, padToBoundary);
			}
		}

		return mUseGlobalMeshSpec ? mMeshSpec : meshSpec;
	}

	UniformCollection ResourceStreamSerializer::readUniformCollection(ifstream& fp)
	{
		UniformCollection uniforms;

		size_t numUniforms = readUInt(fp);
		for (size_t i = 0; i < numUniforms; ++i)
		{
			string name = readString(fp);
			auto type = static_cast<program::GLSLType>(readUInt(fp));
			auto size = readUInt(fp);

			char data[64];
			fp.read(data, 64);

			uniforms.setUniform(name, type, size, data);
		}

		return uniforms;
	}

	shared_ptr<program::Parser> ResourceStreamSerializer::readParser(ifstream& fp)
	{
		auto name = readString(fp);

		auto parser = make_shared<program::Parser>(name);

		// Shader stages
		for (uint32_t i = 0; i < (uint32_t)program::ShaderStage::Type::NumStages; ++i)
		{
			auto source = readString(fp);
			
			switch (i)
			{
			case (uint32_t)program::ShaderStage::Type::Vertex:
				parser->setVertexSource(source);
				break;

			case (uint32_t)program::ShaderStage::Type::Geometry:
				parser->setGeometrySource(source);
				break;

			case (uint32_t)program::ShaderStage::Type::Fragment:
				parser->setFragmentSource(source);
				break;
			}
		}

		auto meshSpec = readMeshSpecification(fp);
		parser->setMeshSpecification(meshSpec);

		return parser;
	}

	void ResourceStreamSerializer::readMaterialStream(ResourceStreamPtr resourceStream, ifstream& fp, map<uint32_t, string> const& qualityNames)
	{
		auto pStream = static_cast<ProgrammaticMaterialStream*>(resourceStream.get());
		pStream->mQualitySettings.clear();

		// Read number of quality settings
		size_t numSettings = readUInt(fp);

		// Read quality settings
		for (size_t i = 0; i < numSettings; ++i)
		{
			string name = qualityNames.at(i);
			auto quality = pStream->createQualitySetting(name);

			// Program options
			auto& qs = pStream->mQualitySettings[quality];

			qs.spec.program.resourceExists = readBool(fp);
			qs.spec.program.existingResource = readString(fp);
			qs.spec.program.isChild = readBool(fp);
			qs.spec.program.is2d = readBool(fp);

			// MeshSpecification
			auto meshSpec = readMeshSpecification(fp);
			qs.spec.program.spec = meshSpec;

			// Shaders
			qs.spec.program.vertexShader.type = static_cast<MaterialSpecification::ProgramOptions::Shader::Type>(readUInt(fp));
			qs.spec.program.vertexShader.data = readString(fp);

			qs.spec.program.geometryShader.type = static_cast<MaterialSpecification::ProgramOptions::Shader::Type>(readUInt(fp));
			qs.spec.program.geometryShader.data = readString(fp);

			qs.spec.program.fragmentShader.type = static_cast<MaterialSpecification::ProgramOptions::Shader::Type>(readUInt(fp));
			qs.spec.program.fragmentShader.data = readString(fp);

			// Uniforms
			qs.spec.uniforms = readUniformCollection(fp);

			// Textures
			size_t numTextures = readUInt(fp);
			for (size_t j = 0; j < numTextures; ++j)
			{
				auto sampler = readString(fp);
				auto resource = readString(fp);
				auto exists = readBool(fp);

				qs.spec.textures[sampler] = make_pair(resource, exists);
			}
		}
	}

	void ResourceStreamSerializer::readProgramStream(ResourceStreamPtr resourceStream, ifstream& fp, map<uint32_t, string> const& qualityNames)
	{
		auto pStream = static_cast<ProgrammaticProgramStream*>(resourceStream.get());
		pStream->mQualitySettings.clear();

		// Read source
		auto vertexSource = readString(fp);
		auto geometrySource = readString(fp);
		auto fragmentSource = readString(fp);

		// Read number of quality settings
		size_t numSettings = readUInt(fp);

		// Read quality settings
		for (size_t i = 0; i < numSettings; ++i)
		{
			string name = qualityNames.at(i);
			auto quality = pStream->createQualitySetting(name);

			auto& qs = pStream->mQualitySettings[quality];

			qs.parser = readParser(fp);

			qs.vertexShader.type = static_cast<ProgramStream::Shader::Type>(readUInt(fp));
			qs.vertexShader.source = readString(fp);
			qs.vertexShader.data = readString(fp);

			qs.geometryShader.type = static_cast<ProgramStream::Shader::Type>(readUInt(fp));
			qs.geometryShader.source = readString(fp);
			qs.geometryShader.data = readString(fp);

			qs.fragmentShader.type = static_cast<ProgramStream::Shader::Type>(readUInt(fp));
			qs.fragmentShader.source = readString(fp);
			qs.fragmentShader.data = readString(fp);
		}

		auto numAttribs = readUInt(fp);
		for (size_t i = 0; i < numAttribs; ++i)
		{
			pStream->mAttribs.insert(readString(fp));
		}
	}

	void ResourceStreamSerializer::readSamplerStream(ResourceStreamPtr resourceStream, ifstream& fp, map<uint32_t, string> const& qualityNames)
	{
		auto pStream = static_cast<ProgrammaticSamplerStream*>(resourceStream.get());
		pStream->mQualitySettings.clear();

		// Read number of quality settings
		size_t numSettings = readUInt(fp);

		// Read quality settings
		for (size_t i = 0; i < numSettings; ++i)
		{
			string name = qualityNames.at(i);
			auto quality = pStream->createQualitySetting(name);

			auto& qs = pStream->mQualitySettings.at(quality);

			qs.params.minFilter = readUInt(fp);
			qs.params.magFilter = readUInt(fp);
			qs.params.wrap = readUInt(fp);
			qs.params.lodMinLevel = readFloat(fp);
			qs.params.lodMaxLevel = readFloat(fp);
			qs.params.lodBias = readFloat(fp);
			qs.params.maxAnisotropy = readFloat(fp);
		}
	}

	void ResourceStreamSerializer::readStringStream(ResourceStreamPtr resourceStream, ifstream& fp, map<uint32_t, string> const& qualityNames)
	{
		auto pStream = static_cast<ProgrammaticStringStream*>(resourceStream.get());
		pStream->mQualitySettings.clear();

		// Read number of quality settings
		size_t numSettings = readUInt(fp);

		// Read quality settings
		for (size_t i = 0; i < numSettings; ++i)
		{
			string name = qualityNames.at(i);
			auto quality = pStream->createQualitySetting(name);

			auto& qs = pStream->mQualitySettings.at(quality);

			qs.data = readString(fp);
			qs.file = readString(fp);
			qs.isFile = readBool(fp);
		}
	}

	void ResourceStreamSerializer::readTextureStream(ResourceStreamPtr resourceStream, ifstream& fp, map<uint32_t, string> const& qualityNames)
	{
		auto pStream = static_cast<ProgrammaticTextureStream*>(resourceStream.get());
		pStream->mQualitySettings.clear();

		// Write tiles
		size_t numTiles = readUInt(fp);
		for (size_t i = 0; i < numTiles; ++i)
		{
			string tileName = readString(fp);
			float u0 = readFloat(fp);
			float u1 = readFloat(fp);
			float v0 = readFloat(fp);
			float v1 = readFloat(fp);

			TextureStream::Tile tile
			{
				{ u0, u1 }, { v0, v1 }
			};

			pStream->mTiles[tileName] = tile;
		}

		// Read number of quality settings
		size_t numSettings = readUInt(fp);

		// Read quality settings
		for (size_t i = 0; i < numSettings; ++i)
		{
			string name = qualityNames.at(i);
			auto quality = pStream->createQualitySetting(name);

			auto& qs = pStream->mQualitySettings.at(quality);

			qs.params.minFilter = readUInt(fp);
			qs.params.magFilter = readUInt(fp);
			qs.params.wrap = readUInt(fp);
			qs.params.useMipmaps = readBool(fp);
			qs.params.lodBaseLevel = readInt(fp);
			qs.params.lodMaxLevel = readInt(fp);
			qs.params.lodBias = readFloat(fp);
			qs.params.maxAnisotropy = readFloat(fp);

			// Other.  Don't read the image load function, this will be provided by whatever loads this.
			qs.sampler = readString(fp);
			qs.source = readString(fp);
		}
	}

	ResourceStreamPtr ResourceStreamSerializer::readStream(ifstream& fp)
	{
		auto streamType = readString(fp);
		ResourceStreamPtr resourceStream;

		if (streamType == "Material")
		{
			resourceStream.reset(new ProgrammaticMaterialStream(mResourceMgr));
		}
		else if (streamType == "Program")
		{
			resourceStream.reset(new ProgrammaticProgramStream(mResourceMgr));
		}
		else if (streamType == "Sampler")
		{
			resourceStream.reset(new ProgrammaticSamplerStream(mResourceMgr));
		}
		else if (streamType == "String")
		{
			resourceStream.reset(new ProgrammaticStringStream(mResourceMgr));
		}
		else if (streamType == "Texture")
		{
			resourceStream.reset(new ProgrammaticTextureStream(mResourceMgr));
		}
		else
		{
			string errMsg = "Cannot deserialize ResourceStream of type '" + streamType + "'.";
			THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
		}

		// Clear the quality setting that gets created
		resourceStream->mQualityNames.clear();

		// Read children
		size_t numChildren = readUInt(fp);
		for (size_t i = 0; i < numChildren; ++i)
		{
			auto name = readString(fp);
			auto res = readStream(fp);

			resourceStream->addChild(name, res);
		}

		// Read number of quality settings
		size_t numSettings = readUInt(fp);

		// Read quality settings names
		map<uint32_t, string> settingNames;
		for (size_t i = 0; i < numSettings; ++i)
		{
			auto name = readString(fp);
			auto id = readUInt(fp);

			settingNames[id] = name;
		}

		// Read type-specific data
		if (streamType == "Material")
		{
			readMaterialStream(resourceStream, fp, settingNames);
		}
		else if (streamType == "Program")
		{
			readProgramStream(resourceStream, fp, settingNames);
		}
		else if (streamType == "Sampler")
		{
			readSamplerStream(resourceStream, fp, settingNames);
		}
		else if (streamType == "String")
		{
			readStringStream(resourceStream, fp, settingNames);
		}
		else if (streamType == "Texture")
		{
			readTextureStream(resourceStream, fp, settingNames);
		}
		else
		{
			string errMsg = "Cannot deserialize ResourceStream of type '" + streamType + "'.";
			THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
		}

		return resourceStream;
	}
}