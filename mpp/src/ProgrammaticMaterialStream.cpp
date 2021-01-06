#include "mpp/DefaultShaders.h"
#include "mpp/Program.h"
#include "mpp/ResourceManager.h"
#include "mpp/ProgrammaticMaterialStream.h"
#include "mpp/ProgrammaticProgramStream.h"
#include "mpp/ProgrammaticTextureStream.h"

using namespace std;

namespace mpp
{

	ProgrammaticMaterialStream::ProgrammaticMaterialStream(ResourceManager* resourceMgr)
		: MaterialStream(resourceMgr)
	{
		createQualitySetting("");
	}

	void ProgrammaticMaterialStream::createChildResourceStreamsImpl()
	{
		auto const& qs = mQualitySettings[mQualitySetting];

		// Program
		if (qs.spec.program.resourceExists && qs.spec.program.isChild)
		{
			auto programStream = new ProgrammaticProgramStream(getResourceMgr());
			
			auto parser = make_shared<program::Parser>();

			parser->setMeshSpecification(qs.spec.program.spec);

			// Load source into parser
			if (qs.spec.program.vertexShader.type == MaterialSpecification::ProgramOptions::Shader::Type::Default)
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

			if (qs.spec.program.fragmentShader.type == MaterialSpecification::ProgramOptions::Shader::Type::Default)
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
				textureStream->setFile(texture.existingResource, resMgr->getImageLoadFunction());

				addChild(texture.existingResource, ResourceStreamPtr(textureStream));
			}
		}
	}

	void ProgrammaticMaterialStream::setSpecification(MaterialSpecification const& matSpec, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec = matSpec;
	}

	/*
	 * Set program
	 *
	 */
	void ProgrammaticMaterialStream::setProgram(MaterialSpecification::ProgramOptions progOptions, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program = progOptions;
	}

	void ProgrammaticMaterialStream::setProgram(string const& program, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.isChild = false;
		qs.spec.program.resourceExists = true;
		qs.spec.program.existingResource = program;
	}

	void ProgrammaticMaterialStream::setMeshSpecification(mesh::MeshSpecification const& meshSpec, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.spec = meshSpec;
	}

	void ProgrammaticMaterialStream::setProgram2d(bool is2d, size_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.is2d = is2d;
	}

	void ProgrammaticMaterialStream::setProgramVertexShaderFile(string const& file, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.vertexShader.type = MaterialSpecification::ProgramOptions::Shader::Type::File;
		qs.spec.program.vertexShader.data = file;
	}

	void ProgrammaticMaterialStream::setProgramVertexShaderResource(string const& resource, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.vertexShader.type = MaterialSpecification::ProgramOptions::Shader::Type::Resource;
		qs.spec.program.vertexShader.data = resource;
	}

	void ProgrammaticMaterialStream::setProgramGeometryShaderFile(string const& file, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.geometryShader.type = MaterialSpecification::ProgramOptions::Shader::Type::File;
		qs.spec.program.geometryShader.data = file;
	}

	void ProgrammaticMaterialStream::setProgramGeometryShaderResource(string const& resource, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.geometryShader.type = MaterialSpecification::ProgramOptions::Shader::Type::Resource;
		qs.spec.program.geometryShader.data = resource;
	}

	void ProgrammaticMaterialStream::setProgramFragmentShaderFile(string const& file, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.fragmentShader.type = MaterialSpecification::ProgramOptions::Shader::Type::File;
		qs.spec.program.fragmentShader.data = file;
	}

	void ProgrammaticMaterialStream::setProgramFragmentShaderResource(string const& resource, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.fragmentShader.type = MaterialSpecification::ProgramOptions::Shader::Type::Resource;
		qs.spec.program.fragmentShader.data = resource;
	}

	void ProgrammaticMaterialStream::setTextureChild(string const& sampler, string const& resource, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		MaterialSpecification::TextureOptions textureOptions;
		
		textureOptions.resourceExists = true;
		textureOptions.sampler = sampler;

		textureOptions.isChild = true;
		textureOptions.existingResource = resource;

		qs.spec.textures.push_back(textureOptions);
	}

	void ProgrammaticMaterialStream::setTexture(string const& sampler, string const& texture, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		MaterialSpecification::TextureOptions textureOptions;

		textureOptions.resourceExists = true;
		textureOptions.sampler = sampler;

		textureOptions.isChild = false;
		textureOptions.existingResource = texture;

		qs.spec.textures.push_back(textureOptions);
	}

	void ProgrammaticMaterialStream::setDefaultTexture(uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		MaterialSpecification::TextureOptions textureOptions;

		textureOptions.existingResource = true;
		textureOptions.sampler = "TEX1";

		textureOptions.isChild = false;
		textureOptions.existingResource = "__mpp_tex_none__";

		qs.spec.textures.push_back(textureOptions);
	}

	void ProgrammaticMaterialStream::setUniforms(UniformCollection const& uniforms, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms = uniforms;
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, int32_t value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, value);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, uint32_t value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, value);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, float value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, value);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, glm::vec3 const& value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, value);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, glm::vec4 const& value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, value);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, size_t count, int32_t const* values, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, count, values);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, size_t count, uint32_t const* values, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, count, values);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, size_t count, float const* values, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, count, values);
	}
}