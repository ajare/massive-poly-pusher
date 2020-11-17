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
	 * Constructor.
	 *
	 */
	ProgrammaticMaterialStream::ProgrammaticMaterialStream(ResourceManager* resourceMgr, string const& program)
		: MaterialStream(resourceMgr)
	{
		createQualitySetting("");
		setProgram(program);
	}

	ProgrammaticMaterialStream::ProgrammaticMaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec, string const& vertexShader, bool vertexShaderIsFile, string const& fragmentShader, bool fragmentShaderIsFile)
		: MaterialStream(resourceMgr)
	{
		createQualitySetting("");
		setProgram(program2d, meshSpec, vertexShader, vertexShaderIsFile, fragmentShader, fragmentShaderIsFile);
	}

	/*
	 * Constructor.
	 *
	 */
	ProgrammaticMaterialStream::ProgrammaticMaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec)
		: MaterialStream(resourceMgr)
	{
		createQualitySetting("");
		setProgram(program2d, meshSpec);
	}

	/*
	 * Constructor.
	 *
	 */
	ProgrammaticMaterialStream::ProgrammaticMaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec, set<string> const& tags)
		: MaterialStream(resourceMgr)
	{
		createQualitySetting("");
		setProgram(program2d, meshSpec, tags);
	}

	/*
	 * Set program
	 *
	 */
	void ProgrammaticMaterialStream::setProgram(string const& program, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.program.resourceExists = true;
		qs.program.existingResource = program;
	}

	/*
	 * Set program
	 *
	 */
	void ProgrammaticMaterialStream::setProgram(bool is2d, mpp::mesh::MeshSpecification const& spec, set<string> const& tags, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.program.resourceExists = true;
		qs.program.is2d = is2d;

		string prefix = "__mpp_";
		qs.program.existingResource = spec.getDescriptor(prefix + (is2d ? "p2d_" : "p3d_"));

		for (auto const& tag : tags)
		{
			if (tag == "diffuse")
			{
				qs.program.existingResource += "_d";
			}
		}

		qs.program.existingResource += "__";
	}

	void ProgrammaticMaterialStream::setProgram(bool is2d, mesh::MeshSpecification const& spec, std::string const& vertexShader, bool vertexShaderIsFile, std::string const& fragmentShader, bool fragmentShaderIsFile, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.program.resourceExists = false;
		qs.program.is2d = is2d;
		qs.program.spec = spec;
		qs.program.vertexShader = { vertexShaderIsFile, vertexShader };
		qs.program.fragmentShader = { fragmentShaderIsFile, fragmentShader };
	}

	void ProgrammaticMaterialStream::setProgram(bool is2d, mesh::MeshSpecification const& spec, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.program.resourceExists = false;
		qs.program.is2d = is2d;
		qs.program.spec = spec;
		qs.program.vertexShader = { false, "" };
		qs.program.fragmentShader = { false, "" };
	}

	void ProgrammaticMaterialStream::setTextureChild(string const& sampler, string const& resource, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.textures[sampler] = make_pair(resource, true);
	}

	void ProgrammaticMaterialStream::setTexture(string const& sampler, string const& texture, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.textures[sampler] = make_pair(texture, false);
	}

	void ProgrammaticMaterialStream::setDefaultTexture(uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.textures["TEX1"] = make_pair("__mpp_tex_none__", false);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, int32_t value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.uniforms.setUniform(name, value);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, uint32_t value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.uniforms.setUniform(name, value);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, float value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.uniforms.setUniform(name, value);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, glm::vec3 const& value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.uniforms.setUniform(name, value);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, glm::vec4 const& value, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.uniforms.setUniform(name, value);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, size_t count, int32_t const* values, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.uniforms.setUniform(name, count, values);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, size_t count, uint32_t const* values, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.uniforms.setUniform(name, count, values);
	}

	void ProgrammaticMaterialStream::setUniform(string const& name, size_t count, float const* values, uint32_t quality)
	{
		auto& qs = mQualitySettings[quality];

		qs.uniforms.setUniform(name, count, values);
	}
}