#include <exception>

#include "utils/StringUtils.h"

#include "mpp/ProgrammaticMaterialStream.h"
#include "mpp/ResourceStreamSerializer.h"

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
							auto mstream = make_shared<resource_parsers::FileMaterialStream>(nullptr, getFilepath(), mentry.second.getEntry("Resource"), mMeshSpec, false);
							mstream->load(0);
							
							// Serialize the materialstream

							// Add material
							if (mMaterials.find(name) != mMaterials.end())
							{
								string errMsg = "Error loading " + getFilepath() + ".  Duplicate material named '" + name + "' specified.";
								throw exception(errMsg.c_str());
							}

							mMaterials[name] = mstream;
						}
					}
				}
			}
		}

		mesh::MeshSpecification const& ModelspecStream::getMeshSpecification() const
		{
			return mMeshSpec;
		}

		map<string, ResourceStreamPtr> const& ModelspecStream::getMaterials() const
		{
			return mMaterials;
		}

		void ModelspecStream::serialize(ofstream& fp)
		{
			// Write all materials using shared functions from ResourceStreamSerializer
			mpp::ResourceStreamSerializer ser(nullptr);

			for (auto const& material: mMaterials)
			{
				ser.serialize(material.second, fp);
			}
		}
	}

}