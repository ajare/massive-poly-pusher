#include "mpp/DefaultShaders.h"
#include "mpp/String.h"

#include "mpp/resource-parsers/FileProgramStream.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"

namespace mpp
{
	namespace resource_parsers
	{

		using namespace std;

		FileProgramStream::FileProgramStream(ResourceManager* resourceMgr, string const& filepath)
			: ProgramStream(resourceMgr)
			, FileStream(filepath)
		{
			setup();
		}

		FileProgramStream::FileProgramStream(ResourceManager* resourceMgr, string const& filepath, utils::StructuredData const& data)
			: ProgramStream(resourceMgr)
			, FileStream(filepath, data)
		{
			setup();
		}

		void FileProgramStream::setup()
		{
			// Primitives
			mMeshSpecificationPrimitive["POINTS"] = mesh::Primitive::Type::Points;
			mMeshSpecificationPrimitive["LINES"] = mesh::Primitive::Type::Lines;
			mMeshSpecificationPrimitive["TRIANGLES"] = mesh::Primitive::Type::Triangles;

			// Storage
			mMeshSpecificationStorage["STATIC"] = mesh::VertexBufferStorageType::Static;
			mMeshSpecificationStorage["DYNAMIC"] = mesh::VertexBufferStorageType::Dynamic;

			mComponentTypes["POSITION2"] = mpp::mesh::Vertex::Component::Position2;
			mComponentTypes["POSITION3"] = mpp::mesh::Vertex::Component::Position3;
			mComponentTypes["POSITION4"] = mpp::mesh::Vertex::Component::Position4;
			mComponentTypes["NORMAL3"] = mpp::mesh::Vertex::Component::Normal3;
			mComponentTypes["NORMAL4"] = mpp::mesh::Vertex::Component::Normal4;
			mComponentTypes["TEXCOORD2"] = mpp::mesh::Vertex::Component::TexCoord2;
			mComponentTypes["TEXCOORD3"] = mpp::mesh::Vertex::Component::TexCoord3;
			mComponentTypes["TEXCOORD4"] = mpp::mesh::Vertex::Component::TexCoord4;
			mComponentTypes["COLOUR1"] = mpp::mesh::Vertex::Component::Colour1;
			mComponentTypes["COLOUR3"] = mpp::mesh::Vertex::Component::Colour3;
			mComponentTypes["COLOUR4"] = mpp::mesh::Vertex::Component::Colour4;
			mComponentTypes["USER1"] = mpp::mesh::Vertex::Component::UserDefined1;
			mComponentTypes["USER2"] = mpp::mesh::Vertex::Component::UserDefined2;
			mComponentTypes["USER3"] = mpp::mesh::Vertex::Component::UserDefined3;
			mComponentTypes["USER4"] = mpp::mesh::Vertex::Component::UserDefined4;

			mDataTypes["FLOAT16"] = mpp::mesh::Vertex::DataType::HalfFloat;
			mDataTypes["FLOAT32"] = mpp::mesh::Vertex::DataType::Float;
			mDataTypes["UINT8"] = mpp::mesh::Vertex::DataType::UnsignedByte;
			mDataTypes["UINT16"] = mpp::mesh::Vertex::DataType::UnsignedShort;
		}

		string FileProgramStream::parseShader(utils::StructuredData const& data)
		{
			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = entry.second.getValue();

				if (entry.first == "file")
				{
					return readTextFile(value);
				}
				else if (entry.first == "resource")
				{
					auto res = getResourceMgr()->getResource(value);
					return dynamic_cast<String*>(res.get())->getData();
				}
			}

			string errMsg = "Error loading " + getFilepath() + ".  Neither 'file' nor 'resource' specified for Shader.";
			THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
		}

		mesh::Vertex::Component FileProgramStream::parseMeshSpecificationBufferChannelComponent(string const& value)
		{
			auto it = mComponentTypes.find(value);

			if (it != mComponentTypes.end())
			{
				return it->second;
			}
			else
			{
				string errMsg = "Error loading " + getFilepath() + ".  Unknown/unsupported component '" + value + "' specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __FUNCTION__);
			}
		}

		mesh::Vertex::DataType FileProgramStream::parseMeshSpecificationBufferChannelType(string const& value)
		{
			auto it = mDataTypes.find(value);

			if (it != mDataTypes.end())
			{
				return it->second;
			}
			else
			{
				string errMsg = "Error loading " + getFilepath() + ".  Unknown/unsupported datatype '" + value + "' specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __FUNCTION__);
			}
		}

		void FileProgramStream::parseMeshSpecificationBufferChannel(utils::StructuredData const& data, mesh::VertexBufferAttributeLayout* layout)
		{
			mesh::Vertex::Component component{ mesh::Vertex::Component::Unused };
			mesh::Vertex::DataType datatype{ mesh::Vertex::DataType::None };
			bool normalised{ false };

			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "data")
				{
					component = parseMeshSpecificationBufferChannelComponent(value);
				}
				else if (entry.first == "type")
				{
					datatype = parseMeshSpecificationBufferChannelType(value);
				}
				else if (entry.first == "normalised")
				{
					normalised = utils::StringUtils::parseBool(value);
				}
			}

			if (component == mesh::Vertex::Component::Unused)
			{
				string errMsg = "Error loading " + getFilepath() + ".  Invalid (or absent) component specified for MeshSpecification.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			if (datatype == mesh::Vertex::DataType::None)
			{
				string errMsg = "Error loading " + getFilepath() + ".  Invalid (or absent) datatype specified for MeshSpecification.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			layout->createAttribute(component, datatype, normalised);
		}

		void FileProgramStream::parseMeshSpecificationBuffer(utils::StructuredData const& data, mesh::MeshSpecification& meshSpec)
		{
			auto buffer = meshSpec.createVertexBufferAttributeLayout(meshSpec.getStorageType() == mesh::VertexBufferStorageType::Static);

			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "Channel")
				{
					parseMeshSpecificationBufferChannel(entry.second, buffer);
				}
			}
		}

		mesh::Primitive::Type FileProgramStream::parseMeshSpecificationPrimitive(string const& value)
		{
			auto it = mMeshSpecificationPrimitive.find(value);

			if (it != mMeshSpecificationPrimitive.end())
			{
				return it->second;
			}
			else
			{
				string errMsg = "Error loading " + getFilepath() + ".  Unknown/unsupported primitive type '" + value + "' specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __FUNCTION__);
			}
		}

		mesh::VertexBufferStorageType FileProgramStream::parseMeshSpecificationStorage(string const& value)
		{
			auto it = mMeshSpecificationStorage.find(value);

			if (it != mMeshSpecificationStorage.end())
			{
				return it->second;
			}
			else
			{
				string errMsg = "Error loading " + getFilepath() + ".  Unknown/unsupported storage type '" + value + "' specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __FUNCTION__);
			}
		}

		mesh::MeshSpecification FileProgramStream::parseMeshSpecification(utils::StructuredData const& data)
		{
			if (data.getName() != "MeshSpecification")
			{
				string errMsg = "Error loading " + getFilepath() + ".  MeshSpecification element is not 'MeshSpecification'.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			mesh::MeshSpecification meshSpec;

			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "primitive")
				{
					meshSpec.setPrimitiveType(parseMeshSpecificationPrimitive(value));
				}
				else if (entry.first == "indexed")
				{
					meshSpec.setIndexedVertices(utils::StringUtils::parseBool(value));
				}
				else if (entry.first == "storage")
				{
					meshSpec.setStorageType(parseMeshSpecificationStorage(value));
				}
				else if (entry.first == "Buffer")
				{
					parseMeshSpecificationBuffer(entry.second, meshSpec);
				}
			}

			return meshSpec;
		}


		void FileProgramStream::parseQualitySetting(utils::StructuredData const& data)
		{
			string name;

			// Read settings required to create a Parser
			string vertexShader{ "" }, geometryShader{ "" }, fragmentShader{ "" }, positionType;
			mesh::MeshSpecification meshSpec;
			size_t numTextures{ 0 };
			bool useDiffuse{ false };
			bool useColours{ false };
			bool isAtlas{ false };
			bool useRotation{ false };
		
			enum class PrimitiveAttrib
			{
				Unspecified,
				Triangles,
				Lines,
				Points
			};

			PrimitiveAttrib primitiveAttib{ PrimitiveAttrib::Unspecified };

			bool parsedMeshSpec{ false };
			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "name")
				{
					name = entry.second.getValue();
				}
				else if (entry.first == "positionType")
				{
					positionType = value;
				}
				else if (entry.first == "textures")
				{
					numTextures = utils::StringUtils::parseUInt(value);
				}
				else if (entry.first == "primitive")
				{
					if (primitiveAttib != PrimitiveAttrib::Unspecified)
					{
						string errMsg = "Error loading " + getFilepath() + ".  'primitive' cannot be specified more than once for program.";
						THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
					}
					
					if (value == "POINTS")
					{
						primitiveAttib = PrimitiveAttrib::Points;
					}
					else if (value == "LINES")
					{
						primitiveAttib = PrimitiveAttrib::Lines;
					}
					else if (value == "TRIANGLES")
					{
						primitiveAttib = PrimitiveAttrib::Triangles;
					}
				}
				else if (entry.first == "diffuse")
				{
					useDiffuse = utils::StringUtils::parseBool(value);
				}
				else if (entry.first == "colours")
				{
					useColours = utils::StringUtils::parseBool(value);
				}
				else if (entry.first == "atlas")
				{
					isAtlas = utils::StringUtils::parseBool(value);
				}
				else if (entry.first == "rotation")
				{
					useRotation = utils::StringUtils::parseBool(value);
				}
				else if (entry.first == "VertexShader")
				{
					vertexShader = parseShader(entry.second);
				}
				else if (entry.first == "GeometryShader")
				{
					THROW_MPP_RESOURCE_PARSERS_NOTIMP("Geometry shaders", __LINE__, __FILE__, __func__);
				}
				else if (entry.first == "FragmentShader")
				{
					fragmentShader = parseShader(entry.second);
				}
				else if (entry.first == "MeshSpecification")
				{
					meshSpec = parseMeshSpecification(entry.second);
					parsedMeshSpec = true;
				}
			}

			if (!parsedMeshSpec)
			{
				string errMsg = "Error loading " + getFilepath() + ".  MeshSpecification not specified for program.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			if (positionType != "2D" && positionType != "3D")
			{
				string errMsg = "Error loading " + getFilepath() + ".  Invalid (or absent) position type specified for program.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			// Set attribs
			if (numTextures > 0)
			{
				mAttribs.insert("Texture");
			}
			if (primitiveAttib != PrimitiveAttrib::Unspecified)
			{
				if (primitiveAttib == PrimitiveAttrib::Points)
				{
					mAttribs.insert("Points");
				}
				else if (primitiveAttib == PrimitiveAttrib::Lines)
				{
					mAttribs.insert("Lines");
				}
				else if (primitiveAttib == PrimitiveAttrib::Triangles)
				{
					mAttribs.insert("Triangles");
				}
			}
			if (useColours)
			{
				mAttribs.insert("Colours");
			}
			if (isAtlas)
			{
				mAttribs.insert("Atlas");
			}
			if (useRotation)
			{
				mAttribs.insert("Rotation");
			}

			// Create quality settings
			auto newSettingId = createQualitySetting(name);
			
			auto& qs = mQualitySettings[newSettingId];

			qs.parser = make_shared<program::Parser>();
			qs.parser->setMeshSpecification(meshSpec);
			
			// Load source into parser
			if (vertexShader == "")
			{
				if (positionType == "2D")
				{
					qs.parser->setVertexSource(VertexShader2dTemplate);
				}
				else if (positionType == "3D")
				{
					qs.parser->setVertexSource(VertexShader3dTemplate);
				}
			}
			else
			{
				qs.parser->setVertexSource(vertexShader);
			}

			if (fragmentShader == "")
			{
				if (positionType == "2D")
				{
					qs.parser->setFragmentSource(FragmentShader2dTemplate);
				}
				else if (positionType == "3D")
				{
					qs.parser->setFragmentSource(FragmentShader3dTemplate);
				}
			}
			else
			{
				qs.parser->setFragmentSource(fragmentShader);
			}
		}

		void FileProgramStream::loadImpl()
		{
			auto const& data = getStructuredData();

			// Parse data.  Root element should be 'Program'
			auto rootName = data.getName();

			if (rootName != "Program" && rootName != "Resource")
			{
				string errMsg = "Error loading " + getFilepath() + ".  Root element is neither 'Program' nor 'Resource'.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			parseQualitySetting(data);

			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "Quality")
				{
					parseQualitySetting(entry.second);
				}
			}

			ProgramStream::loadImpl();
		}
	}
}