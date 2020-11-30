#include "mpp/program/ShaderStage.h"

#include "mpp/ResourceStreamSerializer.h"
#include "mpp/MaterialStream.h"
#include "mpp/ProgramStream.h"
#include "mpp/SamplerStream.h"
#include "mpp/StringStream.h"
#include "mpp/TextureStream.h"
#include "mpp/MppException.h"

namespace mpp
{
	using namespace std;

	void ResourceStreamSerializer::serialize(ResourceStreamPtr resourceStream, string const& filename)
	{
		// Open file
		ofstream fp(filename, ios::out | ios::binary);

		if (!fp.is_open())
		{
			THROW_MPP_IO("Could not open " + filename + " for writing", __LINE__, __FILE__, __func__);
		}

		// Write magic number
		char* magic{ "RSER" };
		fp.write(magic, 4);

		// Recursively write streams and their children
		writeStream(resourceStream, fp);

		fp.close();
	}

	void ResourceStreamSerializer::writeValue(string const& value, ofstream& fp)
	{
		size_t len = value.length();
		fp.write((char const*)&len, sizeof(len));
		fp.write(value.c_str(), len);
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
		writeValue((uint32_t)meshSpec.getPrimitiveType(), fp);
		writeValue((uint32_t)meshSpec.getStorageType(), fp);
		writeValue(meshSpec.verticesIndexed(), fp);

		// Write layouts
		writeValue(meshSpec.getNumVertexBufferAttributeLayouts(), fp);
		for (int i = 0; i < meshSpec.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto const& layout = meshSpec.getVertexBufferAttributeLayout(i);

			// Write attributes
			writeValue(layout.getNumAttributes(), fp);
			for (int j = 0; j < layout.getNumAttributes(); ++j)
			{
				auto const& attrib = layout.getAttribute(j);

				writeValue(attrib.attributeId, fp);
				writeValue(attrib.identifier, fp);
				writeValue((uint32_t)attrib.component, fp);
				writeValue((uint32_t)attrib.dataType, fp);
				writeValue(attrib.paddingBytes, fp);
				writeValue(attrib.normalised, fp);
				writeValue(attrib.offsetInBytes, fp);
			}

			writeValue(layout.getVertexSize(), fp);
			writeValue(layout.getBaseId(), fp);
			writeValue(layout.isStatic(), fp);
		}
	}

	void ResourceStreamSerializer::writeUniformCollection(UniformCollection const& uniforms, ofstream& fp)
	{
		writeValue(uniforms.getNumUniforms(), fp);

		auto const& uniformData = uniforms.getUniformData();

		for (auto const& kvp: uniformData)
		{
			auto const& name = kvp.first;
			auto const& data = kvp.second;

			writeValue(name, fp);

			writeValue(data.name, fp);
			writeValue((uint32_t)data.type, fp);
			writeValue(data.size, fp);

			fp.write(data.data, 64);
		}
	}

	void ResourceStreamSerializer::writeGLSLDecl(program::GLSLTypeDecl decl, ofstream& fp)
	{
		writeValue(decl.name, fp);
		writeValue((uint32_t)decl.type, fp);
		writeValue((uint32_t)decl.dataType, fp);
		writeValue(decl.size[0], fp);
		writeValue(decl.size[1], fp);
		writeValue(decl.isFloatingPoint, fp);
		writeValue(decl.isSigned, fp);
	}

	void ResourceStreamSerializer::writeParser(program::Parser const& parser, ofstream& fp)
	{
		writeValue(parser.getName(), fp);

		// Shader stages
		writeValue((uint32_t)program::ShaderStage::Type::NumStages, fp);
		for (uint32_t i = 0; i < (uint32_t)program::ShaderStage::Type::NumStages; ++i)
		{
			auto const& stage = parser.getStage(i);

			writeValue((uint32_t)stage.type, fp);
			writeValue(stage.source, fp);
			writeValue(stage.generated, fp);
			writeValue(stage.mainLine, fp);

			// Attributes
			writeValue(stage.inAttribs.size(), fp);
			for (auto const& attrib: stage.inAttribs)
			{
				writeValue(attrib.name, fp);
				writeGLSLDecl(attrib.type, fp);
				writeValue(attrib.normalised, fp);
			}

			writeValue(stage.outAttribs.size(), fp);
			for (auto const& attrib: stage.outAttribs)
			{
				writeValue(attrib.name, fp);
				writeGLSLDecl(attrib.type, fp);
				writeValue(attrib.normalised, fp);
			}

			// Uniforms
			writeValue(stage.uniforms.size(), fp);
			for (auto const& uniform: stage.uniforms)
			{
				writeValue(uniform.qualifier, fp);
				writeValue(uniform.name, fp);
				writeGLSLDecl(uniform.type, fp);
			}

			// Textures
			writeValue(stage.textures.size(), fp);
			for (auto const& texture: stage.textures)
			{
				writeValue(texture.name, fp);
				writeGLSLDecl(texture.type, fp);
			}
		}

		writeMeshSpecification(parser.getMeshSpecification(), fp);

		writeValue(parser.getErrors().size(), fp);
		for (auto const& error: parser.getErrors())
		{
			writeValue(error, fp);
		}

		writeValue(parser.getWarnings().size(), fp);
		for (auto const& error: parser.getWarnings())
		{
			writeValue(error, fp);
		}
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
			writeValue(setting.program.resourceExists, fp);
			writeValue(setting.program.existingResource, fp);
			writeValue(setting.program.isChild, fp);
			writeValue(setting.program.is2d, fp);

			writeMeshSpecification(setting.program.spec, fp);

			writeValue((uint32_t)setting.program.vertexShader.type, fp);
			writeValue(setting.program.vertexShader.data, fp);

			// Uniforms
			writeUniformCollection(setting.uniforms, fp);

			// Textures
			writeValue(setting.textures.size(), fp);
			for (auto const& kvp: setting.textures)
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

		// Write texture data
		auto dataSize = stream->mData.width * stream->mData.height * stream->mData.depth * stream->mData.bitsPerPixel / 8;
		writeValue(dataSize, fp);
		fp.write((char const*)stream->mData.data, dataSize);
		writeValue(stream->mData.width, fp);
		writeValue(stream->mData.height, fp);
		writeValue(stream->mData.depth, fp);
		writeValue(stream->mData.bitsPerPixel, fp);
		writeValue(stream->mData.pixelFormat, fp);
		writeValue(stream->mData.dataType, fp);

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

		// Write resource
		auto const& streamType = resourceStream->getType();

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
}