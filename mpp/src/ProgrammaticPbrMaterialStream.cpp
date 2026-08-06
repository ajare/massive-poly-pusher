#include "mpp/DefaultShaders.h"
#include "mpp/Program.h"
#include "mpp/ResourceManager.h"
#include "mpp/ProgrammaticPbrMaterialStream.h"
#include "mpp/ProgrammaticProgramStream.h"
#include "mpp/ProgrammaticTextureStream.h"

using namespace std;

namespace mpp
{

	ProgrammaticPbrMaterialStream::ProgrammaticPbrMaterialStream(ResourceManager* resourceMgr)
		: PbrMaterialStream(resourceMgr)
	{
		createQualitySetting("");
	}

	void ProgrammaticPbrMaterialStream::createChildResourceStreamsImpl()
	{
		auto const& qs = mQualitySettings[mQualitySetting];

		// Program
		if (qs.spec.program.resourceExists && qs.spec.program.isChild)
		{
			auto programStream = new ProgrammaticProgramStream(getResourceMgr());

			auto parser = make_shared<program::Parser>();

			parser->setMeshSpecification(qs.spec.program.spec);

			// Load source into parser
			if (qs.spec.program.vertexShader.type == PbrMaterialSpecification::ProgramOptions::Shader::Type::Default)
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

			if (qs.spec.program.fragmentShader.type == PbrMaterialSpecification::ProgramOptions::Shader::Type::Default)
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

	void ProgrammaticPbrMaterialStream::setSpecification(PbrMaterialSpecification const& matSpec, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec = matSpec;
	}

	/*
	 * Set program
	 *
	 */
	void ProgrammaticPbrMaterialStream::setProgram(PbrMaterialSpecification::ProgramOptions progOptions, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program = progOptions;
	}

	void ProgrammaticPbrMaterialStream::setProgram(string const& program, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.isChild = false;
		qs.spec.program.resourceExists = true;
		qs.spec.program.existingResource = program;
	}

	void ProgrammaticPbrMaterialStream::setMeshSpecification(mesh::MeshSpecification const& meshSpec, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.spec = meshSpec;
	}

	void ProgrammaticPbrMaterialStream::setProgram2d(bool is2d, size_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.is2d = is2d;
	}

	void ProgrammaticPbrMaterialStream::setProgramVertexShaderFile(string const& file, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.vertexShader.type = PbrMaterialSpecification::ProgramOptions::Shader::Type::File;
		qs.spec.program.vertexShader.data = file;
	}

	void ProgrammaticPbrMaterialStream::setProgramVertexShaderResource(string const& resource, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.vertexShader.type = PbrMaterialSpecification::ProgramOptions::Shader::Type::Resource;
		qs.spec.program.vertexShader.data = resource;
	}

	void ProgrammaticPbrMaterialStream::setProgramGeometryShaderFile(string const& file, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.geometryShader.type = PbrMaterialSpecification::ProgramOptions::Shader::Type::File;
		qs.spec.program.geometryShader.data = file;
	}

	void ProgrammaticPbrMaterialStream::setProgramGeometryShaderResource(string const& resource, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.geometryShader.type = PbrMaterialSpecification::ProgramOptions::Shader::Type::Resource;
		qs.spec.program.geometryShader.data = resource;
	}

	void ProgrammaticPbrMaterialStream::setProgramFragmentShaderFile(string const& file, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.fragmentShader.type = PbrMaterialSpecification::ProgramOptions::Shader::Type::File;
		qs.spec.program.fragmentShader.data = file;
	}

	void ProgrammaticPbrMaterialStream::setProgramFragmentShaderResource(string const& resource, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.program.fragmentShader.type = PbrMaterialSpecification::ProgramOptions::Shader::Type::Resource;
		qs.spec.program.fragmentShader.data = resource;
	}

	void ProgrammaticPbrMaterialStream::setTextureChild(string const& sampler, string const& resource, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		PbrMaterialSpecification::TextureOptions textureOptions;

		textureOptions.resourceExists = true;
		textureOptions.sampler = sampler;

		textureOptions.isChild = true;
		textureOptions.existingResource = resource;

		qs.spec.textures.push_back(textureOptions);
	}

	void ProgrammaticPbrMaterialStream::setTexture(string const& sampler, string const& texture, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		PbrMaterialSpecification::TextureOptions textureOptions;

		textureOptions.resourceExists = true;
		textureOptions.sampler = sampler;

		textureOptions.isChild = false;
		textureOptions.existingResource = texture;

		qs.spec.textures.push_back(textureOptions);
	}

	void ProgrammaticPbrMaterialStream::setBaseColourMap(string const& texture, uint32_t quality) { setTexture("PBR_BASE_COLOUR_MAP", texture, quality); }
	void ProgrammaticPbrMaterialStream::setMetallicRoughnessMap(string const& texture, uint32_t quality) { setTexture("PBR_METALLIC_ROUGHNESS_MAP", texture, quality); }
	void ProgrammaticPbrMaterialStream::setNormalMap(string const& texture, uint32_t quality) { setTexture("PBR_NORMAL_MAP", texture, quality); }
	void ProgrammaticPbrMaterialStream::setOcclusionMap(string const& texture, uint32_t quality) { setTexture("PBR_OCCLUSION_MAP", texture, quality); }
	void ProgrammaticPbrMaterialStream::setEmissiveMap(string const& texture, uint32_t quality) { setTexture("PBR_EMISSIVE_MAP", texture, quality); }

	void ProgrammaticPbrMaterialStream::setDefaultTexture(string const& sampler, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		PbrMaterialSpecification::TextureOptions textureOptions;

		textureOptions.existingResource = true;
		textureOptions.sampler = sampler;

		textureOptions.isChild = false;
		textureOptions.existingResource = "__mpp_tex_none__";

		qs.spec.textures.push_back(textureOptions);
	}

	void ProgrammaticPbrMaterialStream::setPbrSurface(PbrMaterialSpecification::PbrSurface const& surface, uint32_t quality)
	{
		mQualitySettings[quality].spec.pbr = surface;
	}

	void ProgrammaticPbrMaterialStream::setUniforms(UniformCollection const& uniforms, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms = uniforms;
	}

	void ProgrammaticPbrMaterialStream::setUniform(string const& name, int32_t value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, value);
	}

	void ProgrammaticPbrMaterialStream::setUniform(string const& name, uint32_t value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, value);
	}

	void ProgrammaticPbrMaterialStream::setUniform(string const& name, float value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, value);
	}

	void ProgrammaticPbrMaterialStream::setUniform(string const& name, glm::vec3 const& value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, value);
	}

	void ProgrammaticPbrMaterialStream::setUniform(string const& name, glm::vec4 const& value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, value);
	}

	void ProgrammaticPbrMaterialStream::setUniform(string const& name, size_t count, int32_t const* values, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, count, 1, values);
	}

	void ProgrammaticPbrMaterialStream::setUniform(string const& name, size_t count, uint32_t const* values, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, count, 1, values);
	}

	void ProgrammaticPbrMaterialStream::setUniform(string const& name, size_t count, float const* values, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.uniforms.setUniform(name, count, 1, values);
	}
}