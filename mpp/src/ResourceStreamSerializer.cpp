#include "mpp/program/ShaderStage.h"

#include "mpp/ResourceStreamSerializer.h"
#include "mpp/BasicMaterialStream.h"
#include "mpp/ProgramStream.h"
#include "mpp/SamplerStream.h"
#include "mpp/StringStream.h"
#include "mpp/TextureStream.h"
#include "mpp/ProgrammaticBasicMaterialStream.h"
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
		char const* magic{ "RSER" };
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
		uint32_t len = (uint32_t)value.length();
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
		writeValue((uint32_t)ms.getNumVertexBufferAttributeLayouts(), fp);
		for (size_t i = 0; i < ms.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto const& layout = ms.getVertexBufferAttributeLayout((uint32_t)i);

			writeValue(layout.isStatic(), fp);

			// Write attributes
			writeValue((uint32_t)layout.getNumAttributes(), fp);
			for (size_t j = 0; j < layout.getNumAttributes(); ++j)
			{
				auto const& attrib = layout.getAttribute(j);

				writeValue((uint32_t)attrib.component, fp);
				writeValue((uint32_t)attrib.dataType, fp);
				writeValue((uint32_t)attrib.padToBoundary, fp);
				writeValue(attrib.normalised, fp);
			}
		}
	}

	void ResourceStreamSerializer::writeUniformCollection(UniformCollection const& uniforms, ofstream& fp)
	{
		writeValue((uint32_t)uniforms.getNumUniforms(), fp);

		auto const& uniformData = uniforms.getUniformData();

		for (auto const& kvp: uniformData)
		{
			auto const& data = kvp.second;

			writeValue(data.name, fp);
			writeValue((uint32_t)data.type, fp);
			writeValue((uint32_t)data.size, fp);

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

	void ResourceStreamSerializer::writeBasicMaterialStream(ResourceStreamPtr resourceStream, ofstream& fp)
	{
		auto stream = dynamic_cast<BasicMaterialStream*>(resourceStream.get());

		// Write number of quality settings
		writeValue((uint32_t)stream->mQualitySettings.size(), fp);

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
			writeValue((uint32_t)setting.spec.textures.size(), fp);
			for (auto const& texture: setting.spec.textures)
			{
				writeValue(texture.resourceExists, fp);
				writeValue(texture.existingResource, fp);
				writeValue(texture.isChild, fp);
				writeValue(texture.sampler, fp);
				writeValue(texture.source, fp);
				writeValue((uint32_t)texture.target, fp);

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
	}

	void ResourceStreamSerializer::writeProgramStream(ResourceStreamPtr resourceStream, ofstream& fp)
	{
		auto stream = dynamic_cast<ProgramStream*>(resourceStream.get());

		// Write source
		writeValue(stream->mVertexSource, fp);
		writeValue(stream->mGeometrySource, fp);
		writeValue(stream->mFragmentSource, fp);

		// Write number of quality settings
		writeValue((uint32_t)stream->mQualitySettings.size(), fp);

		// Write quality settings
		for (auto const& setting: stream->mQualitySettings)
		{
			writeParser(*setting.parser.get(), fp);

			// Write attributes
			writeValue((uint32_t)setting.attribs.size(), fp);
			for (auto const& attrib : setting.attribs)
			{
				writeValue(attrib, fp);
			}
		}

	}

	void ResourceStreamSerializer::writeSamplerStream(ResourceStreamPtr resourceStream, ofstream& fp)
	{
		auto stream = dynamic_cast<SamplerStream*>(resourceStream.get());

		// Write number of quality settings
		writeValue((uint32_t)stream->mQualitySettings.size(), fp);

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
		writeValue((uint32_t)stream->mQualitySettings.size(), fp);

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

		// Compatibility placeholder for the removed tile list. The reader still
		// consumes this count when loading legacy and newly exported models.
		writeValue((uint32_t)0, fp);

		// Write number of quality settings
		writeValue((uint32_t)stream->mQualitySettings.size(), fp);

		// Write quality settings
		for (auto const& setting: stream->mQualitySettings)
		{
			writeValue(setting.target, fp);
			writeValue(setting.internalFormat, fp);

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
	}

	void ResourceStreamSerializer::writeStream(ResourceStreamPtr resourceStream, ofstream& fp)
	{
		// Write type
		auto const& streamType = resourceStream->getType();
		writeValue(streamType, fp);

		// Write children
		auto const& children = resourceStream->getChildren();

		writeValue((uint32_t)children.size(), fp);
		for (auto const& kvp: children)
		{
			auto const& name = kvp.first;
			auto const& child = kvp.second;

			writeValue(name, fp);
			writeStream(child, fp);
		}

		// Write number of quality settings
		writeValue((uint32_t)resourceStream->mQualityNames.size(), fp);

		// Write quality settings names
		for (auto const& kvp: resourceStream->mQualityNames)
		{
			auto const& name = kvp.first;
			auto id = kvp.second;

			writeValue(name, fp);
			writeValue(id, fp);
		}

		// Write type-specific data
		if (streamType == "BasicMaterial" || streamType == "Material")
		{
			writeBasicMaterialStream(resourceStream, fp);
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
		uint32_t len;
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

		uint32_t numLayouts = readUInt(fp);
		for (uint32_t i = 0; i < numLayouts; ++i)
		{
			auto isStatic = readBool(fp);

			auto layout = meshSpec.createVertexBufferAttributeLayout(isStatic);

			uint32_t numAttribs = readUInt(fp);
			for (uint32_t j = 0; j < numAttribs; ++j)
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

		uint32_t numUniforms = readUInt(fp);
		for (uint32_t i = 0; i < numUniforms; ++i)
		{
			string name = readString(fp);
			auto type = static_cast<program::GLSLType>(readUInt(fp));
			auto size = readUInt(fp);

			char data[64];
			fp.read(data, 64);

			// The legacy stream stores a value's byte size, rather than count and
			// component count. Decode a single scalar/vector value from that size.
			size_t componentSize = 0;
			switch (type)
			{
			case program::GLSLType::Bool:
			case program::GLSLType::Int:
			case program::GLSLType::Uint:
			case program::GLSLType::Float:
				componentSize = 4;
				break;
			case program::GLSLType::Double:
				componentSize = 8;
				break;
			default:
				THROW_MPP("Unsupported serialized uniform type.", __LINE__, __FILE__, __func__);
			}
			if (size == 0 || size > sizeof(data) || size % componentSize != 0)
			{
				THROW_MPP("Invalid serialized uniform size.", __LINE__, __FILE__, __func__);
			}
			size_t numElements = size / componentSize;

			// UniformData stores the generated GLSL name. setUniform() adds that
			// markup itself, so convert serialized names back to their authored
			// form rather than generating a double-prefixed uniform name.
			constexpr char uniformPrefix[] = "_mpp_u_";
			const size_t prefixLength = sizeof(uniformPrefix) - 1;
			if (name.size() > prefixLength && name.compare(0, prefixLength, uniformPrefix) == 0 && name.back() == '_')
			{
				name = name.substr(prefixLength, name.size() - prefixLength - 1);
			}
			uniforms.setUniform(name, type, 1, numElements, data);
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

	void ResourceStreamSerializer::readBasicMaterialStream(ResourceStreamPtr resourceStream, ifstream& fp, map<uint32_t, string> const& qualityNames)
	{
		auto pStream = static_cast<ProgrammaticBasicMaterialStream*>(resourceStream.get());
		pStream->mQualitySettings.clear();

		// Read number of quality settings
		uint32_t numSettings = readUInt(fp);

		// Read quality settings
		for (uint32_t i = 0; i < numSettings; ++i)
		{
			string name = qualityNames.at((uint32_t)i);
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
			qs.spec.program.vertexShader.type = static_cast<BasicMaterialSpecification::ProgramOptions::Shader::Type>(readUInt(fp));
			qs.spec.program.vertexShader.data = readString(fp);

			qs.spec.program.geometryShader.type = static_cast<BasicMaterialSpecification::ProgramOptions::Shader::Type>(readUInt(fp));
			qs.spec.program.geometryShader.data = readString(fp);

			qs.spec.program.fragmentShader.type = static_cast<BasicMaterialSpecification::ProgramOptions::Shader::Type>(readUInt(fp));
			qs.spec.program.fragmentShader.data = readString(fp);

			// Uniforms
			qs.spec.uniforms = readUniformCollection(fp);

			// Textures
			uint32_t numTextures = readUInt(fp);
			for (uint32_t j = 0; j < numTextures; ++j)
			{
				BasicMaterialSpecification::TextureOptions textureOptions;

				textureOptions.resourceExists = readBool(fp);
				textureOptions.existingResource = readString(fp);
				textureOptions.isChild = readBool(fp);
				textureOptions.sampler = readString(fp);
				textureOptions.source = readString(fp);
				textureOptions.target = static_cast<TextureTarget>(readUInt(fp));

				// TextureParams
				textureOptions.params.minFilter = readUInt(fp);
				textureOptions.params.magFilter = readUInt(fp);
				textureOptions.params.wrap = readUInt(fp);
				textureOptions.params.useMipmaps = readBool(fp);
				textureOptions.params.lodBaseLevel = readInt(fp);
				textureOptions.params.lodMaxLevel = readInt(fp);
				textureOptions.params.lodBias = readFloat(fp);
				textureOptions.params.maxAnisotropy = readFloat(fp);

				qs.spec.textures.push_back(textureOptions);
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
		uint32_t numSettings = readUInt(fp);

		// Read quality settings
		for (uint32_t i = 0; i < numSettings; ++i)
		{
			string name = qualityNames.at((uint32_t)i);
			auto quality = pStream->createQualitySetting(name);

			auto& qs = pStream->mQualitySettings[quality];

			qs.parser = readParser(fp);

			auto numAttribs = readUInt(fp);
			for (uint32_t i = 0; i < numAttribs; ++i)
			{
				qs.attribs.insert(readString(fp));
			}
		}
	}

	void ResourceStreamSerializer::readSamplerStream(ResourceStreamPtr resourceStream, ifstream& fp, map<uint32_t, string> const& qualityNames)
	{
		auto pStream = static_cast<ProgrammaticSamplerStream*>(resourceStream.get());
		pStream->mQualitySettings.clear();

		// Read number of quality settings
		uint32_t numSettings = readUInt(fp);

		// Read quality settings
		for (uint32_t i = 0; i < numSettings; ++i)
		{
			string name = qualityNames.at((uint32_t)i);
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
		uint32_t numSettings = readUInt(fp);

		// Read quality settings
		for (uint32_t i = 0; i < numSettings; ++i)
		{
			string name = qualityNames.at((uint32_t)i);
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

		// Read tiles.
		// NOTE: tiles have been removed, but this code has been left in to load existing models which were saved with tile information.
		//       new models will be written without.
		uint32_t numTiles = readUInt(fp);
		for (uint32_t i = 0; i < numTiles; ++i)
		{
			string tileName = readString(fp);
			float u0 = readFloat(fp);
			float u1 = readFloat(fp);
			float v0 = readFloat(fp);
			float v1 = readFloat(fp);

			/*
			TextureStream::Tile tile
			{
				{ u0, u1 }, { v0, v1 }
			};

			pStream->mTiles[tileName] = tile;
			*/
		}

		// Read number of quality settings
		uint32_t numSettings = readUInt(fp);

		// Read quality settings
		for (uint32_t i = 0; i < numSettings; ++i)
		{
			string name = qualityNames.at((uint32_t)i);
			auto quality = pStream->createQualitySetting(name);

			auto& qs = pStream->mQualitySettings.at(quality);

			qs.target = readUInt(fp);
			qs.internalFormat = readUInt(fp);

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

		if (streamType == "BasicMaterial" || streamType == "Material")
		{
			resourceStream.reset(new ProgrammaticBasicMaterialStream(mResourceMgr));
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
		uint32_t numChildren = readUInt(fp);
		for (uint32_t i = 0; i < numChildren; ++i)
		{
			auto name = readString(fp);
			auto res = readStream(fp);

			resourceStream->addChild(name, res);
		}

		// Read number of quality settings
		uint32_t numSettings = readUInt(fp);

		// Read quality settings names
		map<uint32_t, string> settingNames;
		for (uint32_t i = 0; i < numSettings; ++i)
		{
			auto name = readString(fp);
			auto id = readUInt(fp);

			settingNames[id] = name;
		}

		// Read type-specific data
		if (streamType == "BasicMaterial" || streamType == "Material")
		{
			readBasicMaterialStream(resourceStream, fp, settingNames);
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