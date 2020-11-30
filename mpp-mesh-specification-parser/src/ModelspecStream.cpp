#include <exception>

#include "utils/StringUtils.h"

#include "mpp/resource-parsers/MeshSpecificationParser.h"
#include "mpp/resource-parsers/FileMaterialStream.h"

#include "mpp/mesh-specification-parser/ModelspecStream.h"

namespace mpp
{
	namespace mesh_specification_parser
	{

		using namespace std;

		ModelspecStream::ModelspecStream(string const& filepath)
			: FileStream(filepath)
		{
		}

		void ModelspecStream::load()
		{
			auto const& data = getStructuredData();

			// Parse data.  Root element should be 'ModelSpecification'
			auto rootName = data.getName();

			if (rootName != "ModelSpecification" && rootName != "Resource")
			{
				string errMsg = "Error loading " + getFilepath() + ".  Root element is neither 'ModelSpecification' nor 'Resource'.";
				throw exception(errMsg.c_str());
			}

			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "MeshSpecification")
				{
					resource_parsers::MeshSpecificationParser parser(getFilepath());
					mMeshSpec = parser.parse(entry.second);
				}
				else if (entry.first == "Materials")
				{
					auto const& materials = entry.second;
					for (auto mit = materials.begin(); mit != materials.end(); ++mit)
					{
						auto const& mentry = *mit;
						if (mentry.first == "Material")
						{
							// Get name
							auto name = mentry.second.getEntry("name").getValue();

							// Get resource
							resource_parsers::FileMaterialStream mstream(nullptr, getFilepath(), mentry.second.getEntry("Resource"));
							mstream.load(0);
							
							// Add material
							if (mMaterials.find(name) != mMaterials.end())
							{
								string errMsg = "Error loading " + getFilepath() + ".  Duplicate material named '" + name + "' specified.";
								throw exception(errMsg.c_str());
							}

							mMaterials[name] = Material();

							// Set up added material
							Material& mat = mMaterials[name];

							auto const& progOpts = mstream.getProgramOptions();

							mat.program.is2d = progOpts.is2d;
							mat.program.resourceExists = progOpts.resourceExists;
							mat.program.existingResource = progOpts.existingResource;

							if (progOpts.resourceExists)
							{
								if (progOpts.isChild)
								{
									// Resource is being declared, so get its name
									auto const& children = mstream.getChildren();
									auto programRes = dynamic_cast<ProgramStream*>(children.at("Program").get());
									programRes->load(0);

									auto const& vertexShader = programRes->getVertexShader();
									auto const& geometryShader = programRes->getGeometryShader();
									auto const& fragmentShader = programRes->getFragmentShader();

									mat.program.vertexShader.data = vertexShader.data;
									switch (vertexShader.type)
									{
									case mpp::ProgramStream::Shader::Type::Default:
										mat.program.vertexShader.type = ProgramOptions::Shader::Type::Default;
										break;

									case mpp::ProgramStream::Shader::Type::File:
										mat.program.vertexShader.type = ProgramOptions::Shader::Type::File;
										break;

									case mpp::ProgramStream::Shader::Type::Resource:
										mat.program.vertexShader.type = ProgramOptions::Shader::Type::Resource;
										break;
									}

									mat.program.geometryShader.data = geometryShader.data;
									switch (geometryShader.type)
									{
									case mpp::ProgramStream::Shader::Type::Default:
										mat.program.geometryShader.type = ProgramOptions::Shader::Type::Default;
										break;

									case mpp::ProgramStream::Shader::Type::File:
										mat.program.geometryShader.type = ProgramOptions::Shader::Type::File;
										break;

									case mpp::ProgramStream::Shader::Type::Resource:
										mat.program.geometryShader.type = ProgramOptions::Shader::Type::Resource;
										break;
									}

									mat.program.fragmentShader.data = fragmentShader.data;
									switch (fragmentShader.type)
									{
									case mpp::ProgramStream::Shader::Type::Default:
										mat.program.fragmentShader.type = ProgramOptions::Shader::Type::Default;
										break;

									case mpp::ProgramStream::Shader::Type::File:
										mat.program.fragmentShader.type = ProgramOptions::Shader::Type::File;
										break;

									case mpp::ProgramStream::Shader::Type::Resource:
										mat.program.fragmentShader.type = ProgramOptions::Shader::Type::Resource;
										break;
									}

								}
							}

							mat.uniforms = mstream.getUniforms();
							mat.textures = mstream.getTextures();
						}
					}
				}
			}
		}

		mesh::MeshSpecification const& ModelspecStream::getMeshSpecification() const
		{
			return mMeshSpec;
		}

		map<string, ModelspecStream::Material> const& ModelspecStream::getMaterials() const
		{
			return mMaterials;
		}
	}
}