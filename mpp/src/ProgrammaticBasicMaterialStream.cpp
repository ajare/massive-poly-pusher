#include "mpp/DefaultShaders.h"
#include "mpp/Program.h"
#include "mpp/ResourceManager.h"
#include "mpp/ProgrammaticBasicMaterialStream.h"
#include "mpp/ProgrammaticProgramStream.h"
#include "mpp/ProgrammaticTextureStream.h"

using namespace std;

namespace mpp
{

	ProgrammaticBasicMaterialStream::ProgrammaticBasicMaterialStream(ResourceManager* resourceMgr)
		: BasicMaterialStream(resourceMgr)
	{
		createQualitySetting("");
	}

	void ProgrammaticBasicMaterialStream::createChildResourceStreamsImpl()
	{
		auto const& qs = mQualitySettings[mQualitySetting];

		// Program
		if (qs.spec.program.resourceExists && qs.spec.program.isChild)
		{
			auto programStream = new ProgrammaticProgramStream(getResourceMgr());
			
			auto parser = make_shared<program::Parser>();

			parser->setMeshSpecification(qs.spec.program.spec);

			// Load source into parser
			if (qs.spec.program.vertexShader.type == BasicMaterialSpecification::ProgramOptions::Shader::Type::Default)
			{
				if (qs.spec.program.is2d)
				{
					parser->setVertexSource(VertexShader2dTemplate);
				}
				else
				{
					parser->setVertexSource(VertexShader3dTemplate);
				}
			}
			else
			{
				parser->setVertexSource(qs.spec.program.vertexShader.data);
			}

			if (qs.spec.program.fragmentShader.type == BasicMaterialSpecification::ProgramOptions::Shader::Type::Default)
			{
				if (qs.spec.program.is2d)
				{
					parser->setFragmentSource(FragmentShader2dTemplate);
				}
				else
				{
					parser->setFragmentSource(FragmentShader3dTemplate);
				}
			}
			else
			{
				parser->setFragmentSource(qs.spec.program.fragmentShader.data);
			}

			programStream->setParser(parser);
			addChild("Program", ResourceStreamPtr(programStream));
		}

		// Textures
		for (auto const& texture: qs.spec.textures)
		{
			if (texture.resourceExists && texture.isChild)
			{
				auto resMgr = getResourceMgr();
				auto textureStream = new ProgrammaticTextureStream(resMgr);

				textureStream->setParams(texture.params);
				textureStream->setSampler(texture.sampler);
				textureStream->setTarget(texture.target);
				textureStream->setFile(texture.source, resMgr->getImageLoadFunction());

				addChild(texture.existingResource, ResourceStreamPtr(textureStream));
			}
		}
	}

	void ProgrammaticBasicMaterialStream::setSpecification(BasicMaterialSpecification const& matSpec, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec = matSpec;
	}

	/*
	 * Set program
	 *
	 */
	void ProgrammaticBasicMaterialStream::setProgram(BasicMaterialSpecification::ProgramOptions progOptions, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program = progOptions;
	}

	void ProgrammaticBasicMaterialStream::setProgram(string const& program, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.isChild = false;
		qs.spec.program.resourceExists = true;
		qs.spec.program.existingResource = program;
	}

	void ProgrammaticBasicMaterialStream::setMeshSpecification(mesh::MeshSpecification const& meshSpec, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.spec = meshSpec;
	}

	void ProgrammaticBasicMaterialStream::setProgram2d(bool is2d, size_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.is2d = is2d;
	}

	void ProgrammaticBasicMaterialStream::setProgramVertexShaderFile(string const& file, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.vertexShader.type = BasicMaterialSpecification::ProgramOptions::Shader::Type::File;
		qs.spec.program.vertexShader.data = file;
	}

	void ProgrammaticBasicMaterialStream::setProgramVertexShaderResource(string const& resource, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.vertexShader.type = BasicMaterialSpecification::ProgramOptions::Shader::Type::Resource;
		qs.spec.program.vertexShader.data = resource;
	}

	void ProgrammaticBasicMaterialStream::setProgramGeometryShaderFile(string const& file, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.geometryShader.type = BasicMaterialSpecification::ProgramOptions::Shader::Type::File;
		qs.spec.program.geometryShader.data = file;
	}

	void ProgrammaticBasicMaterialStream::setProgramGeometryShaderResource(string const& resource, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.geometryShader.type = BasicMaterialSpecification::ProgramOptions::Shader::Type::Resource;
		qs.spec.program.geometryShader.data = resource;
	}

	void ProgrammaticBasicMaterialStream::setProgramFragmentShaderFile(string const& file, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.fragmentShader.type = BasicMaterialSpecification::ProgramOptions::Shader::Type::File;
		qs.spec.program.fragmentShader.data = file;
	}

	void ProgrammaticBasicMaterialStream::setProgramFragmentShaderResource(string const& resource, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.fragmentShader.type = BasicMaterialSpecification::ProgramOptions::Shader::Type::Resource;
		qs.spec.program.fragmentShader.data = resource;
	}

	void ProgrammaticBasicMaterialStream::setTextureChild(string const& sampler, string const& resource, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		BasicMaterialSpecification::TextureOptions textureOptions;
		
		textureOptions.resourceExists = true;
		textureOptions.sampler = sampler;

		textureOptions.isChild = true;
		textureOptions.existingResource = resource;

		qs.spec.textures.push_back(textureOptions);
	}

	void ProgrammaticBasicMaterialStream::setTexture(string const& sampler, string const& texture, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		BasicMaterialSpecification::TextureOptions textureOptions;

		textureOptions.resourceExists = true;
		textureOptions.sampler = sampler;

		textureOptions.isChild = false;
		textureOptions.existingResource = texture;

		qs.spec.textures.push_back(textureOptions);
	}

	void ProgrammaticBasicMaterialStream::setDefaultTexture(string const& sampler, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		BasicMaterialSpecification::TextureOptions textureOptions;

		textureOptions.existingResource = true;
		textureOptions.sampler = sampler;

		textureOptions.isChild = false;
		textureOptions.existingResource = "__mpp_tex_none__";

		qs.spec.textures.push_back(textureOptions);
	}

	void ProgrammaticBasicMaterialStream::setUniforms(UniformCollection const& uniforms, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms = uniforms;
	}

	void ProgrammaticBasicMaterialStream::setUniform(string const& name, int32_t value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, value);
	}

	void ProgrammaticBasicMaterialStream::setUniform(string const& name, uint32_t value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, value);
	}

	void ProgrammaticBasicMaterialStream::setUniform(string const& name, float value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, value);
	}

	void ProgrammaticBasicMaterialStream::setUniform(string const& name, glm::vec3 const& value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, value);
	}

	void ProgrammaticBasicMaterialStream::setUniform(string const& name, glm::vec4 const& value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, value);
	}

	void ProgrammaticBasicMaterialStream::setUniform(string const& name, size_t count, int32_t const* values, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, count, 1, values);
	}

	void ProgrammaticBasicMaterialStream::setUniform(string const& name, size_t count, uint32_t const* values, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, count, 1, values);
	}

	void ProgrammaticBasicMaterialStream::setUniform(string const& name, size_t count, float const* values, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, count, 1, values);
	}
}