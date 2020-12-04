#include "mpp/DefaultShaders.h"
#include "mpp/String.h"

#include "mpp/resource-parsers/FileProgramStream.h"
#include "mpp/resource-parsers/MeshSpecificationParser.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"

namespace mpp
{
	namespace resource_parsers
	{

		using namespace std;

		FileProgramStream::FileProgramStream(ResourceManager* resourceMgr, string const& filepath)
			: ProgramStream(resourceMgr)
			, FileStream(filepath)
			, mMeshSpecRequired(true)
		{
			setup();
		}

		FileProgramStream::FileProgramStream(ResourceManager* resourceMgr, string const& filepath, utils::StructuredData const& data)
			: ProgramStream(resourceMgr)
			, FileStream(filepath, data)
			, mMeshSpecRequired(true)
		{
			setup();
		}

		FileProgramStream::FileProgramStream(ResourceManager* resourceMgr, string const& filepath, mesh::MeshSpecification const& meshSpec)
			: ProgramStream(resourceMgr)
			, FileStream(filepath)
			, mMeshSpecRequired(false)
			, mMeshSpecification(meshSpec)
		{
			setup();
		}

		FileProgramStream::FileProgramStream(ResourceManager* resourceMgr, string const& filepath, utils::StructuredData const& data, mesh::MeshSpecification const& meshSpec)
			: ProgramStream(resourceMgr)
			, FileStream(filepath, data)
			, mMeshSpecRequired(false)
			, mMeshSpecification(meshSpec)
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

		FileProgramStream::Shader FileProgramStream::parseShader(utils::StructuredData const& data)
		{
			Shader shader;

			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = entry.second.getValue();

				if (entry.first == "file")
				{
					shader.type = Shader::Type::File;
					shader.source = value;
					shader.data = readTextFile(value);
				}
				else if (entry.first == "resource")
				{
					shader.type = Shader::Type::Resource;
					shader.source = value;

					auto resourceMgr = getResourceMgr();
					if (resourceMgr)
					{
						auto res = resourceMgr->getResource(value);
						shader.data = dynamic_cast<String*>(res.get())->getData();
					}
				}
			}

			if (shader.type == Shader::Type::Default)
			{
				string errMsg = "Error loading " + getFilepath() + ".  Neither 'file' nor 'resource' specified for Shader.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			return shader;
		}

		void FileProgramStream::parseQualitySetting(utils::StructuredData const& data)
		{
			string name;

			// Read settings required to create a Parser
			Shader vertexShader, geometryShader, fragmentShader;

			string positionType;
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
					MeshSpecificationParser parser(getFilepath());
					meshSpec = parser.parse(entry.second);
					parsedMeshSpec = true;
				}
			}

			if (!parsedMeshSpec&& mMeshSpecRequired)
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

			if (mMeshSpecRequired)
			{
				qs.parser->setMeshSpecification(meshSpec);
			}
			else
			{
				qs.parser->setMeshSpecification(mMeshSpecification);
			}
			
			// Load source into parser
			if (vertexShader.type == Shader::Type::Default)
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
				qs.parser->setVertexSource(vertexShader.data);
			}

			if (fragmentShader.type == Shader::Type::Default)
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
				qs.parser->setFragmentSource(fragmentShader.data);
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