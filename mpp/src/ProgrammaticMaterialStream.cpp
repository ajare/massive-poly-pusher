#include "mpp/Program.h"
#include "mpp/ProgrammaticMaterialStream.h"

using namespace std;

namespace mpp
{

	ProgrammaticMaterialStream::ProgrammaticMaterialStream(ResourceManager* resourceMgr)
		: MaterialStream(resourceMgr)
	{
		createQualitySetting("");
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

		qs.spec.textures[sampler] = make_pair(resource, true);
	}

	void ProgrammaticMaterialStream::setTexture(string const& sampler, string const& texture, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.textures[sampler] = make_pair(texture, false);
	}

	void ProgrammaticMaterialStream::setDefaultTexture(uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.spec.textures["TEX1"] = make_pair("__mpp_tex_none__", false);
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