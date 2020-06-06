#include <stack>

#include "utils/StringUtils.h"

#include "mpp/mesh-specification-parser/SpecificationParser.h"

namespace mpp
{
	namespace mesh_specification_parser
	{

		using namespace std;

		SpecificationParser::SpecificationParser(string const& filename)
			: mFilename(filename)
		{
			mComponentTypes["position2"] = mpp::mesh::Vertex::Component::Position2;
			mComponentTypes["position3"] = mpp::mesh::Vertex::Component::Position3;
			mComponentTypes["position4"] = mpp::mesh::Vertex::Component::Position4;
			mComponentTypes["normal3"] = mpp::mesh::Vertex::Component::Normal3;
			mComponentTypes["normal4"] = mpp::mesh::Vertex::Component::Normal4;
			mComponentTypes["texcoord2"] = mpp::mesh::Vertex::Component::TexCoord2;
			mComponentTypes["texcoord3"] = mpp::mesh::Vertex::Component::TexCoord3;
			mComponentTypes["texcoord4"] = mpp::mesh::Vertex::Component::TexCoord4;
			mComponentTypes["colour1"] = mpp::mesh::Vertex::Component::Colour1;
			mComponentTypes["colour3"] = mpp::mesh::Vertex::Component::Colour3;
			mComponentTypes["colour4"] = mpp::mesh::Vertex::Component::Colour4;

			mDataTypes["float32"] = mpp::mesh::Vertex::DataType::Float;
			mDataTypes["float16"] = mpp::mesh::Vertex::DataType::HalfFloat;
			mDataTypes["uint8"] = mpp::mesh::Vertex::DataType::UnsignedByte;
			mDataTypes["uint16"] = mpp::mesh::Vertex::DataType::UnsignedShort;
		}

		map<string, ProgramInformation> SpecificationParser::parseProgramInformation()
		{
			map<string, ProgramInformation> pinfos;

			utils::XmlReader* reader = utils::XmlReader::fromFile(mFilename);

			auto programNode = reader->getNode("Specification/Programs/Program");
			do
			{
				string name = programNode->getAttribute("name");
				ProgramInformation pi(name);

				// Parse shaders
				auto vsNode = programNode->getOptionalChild("VertexShader");
				if (vsNode)
				{
					// Set vertex shader
					pi.setVertexShader(vsNode->getValue());
				}

				auto fsNode = programNode->getOptionalChild("FragmentShader");
				if (fsNode)
				{
					// Set fragment shader
					pi.setFragmentShader(vsNode->getValue());
				}

				// Parse textures
				auto texturesNode = programNode->getOptionalChild("Textures");
				if (texturesNode)
				{
					auto textureNode = texturesNode->getOptionalChild("Texture");
					if (textureNode)
					{
						do
						{
							pi.setTexture(textureNode->getAttribute("binding"), textureNode->getAttribute("value"));
						} while (textureNode->next());
					}
				}

				// Parse uniforms
				auto uniformsNode = programNode->getOptionalChild("Uniforms");
				if (uniformsNode)
				{
					auto uniformNode = uniformsNode->getOptionalChild("Uniform");
					if (uniformNode)
					{
						do
						{
							pi.setUniform(uniformNode->getAttribute("binding"), uniformNode->getAttribute("value"));
						} while (uniformNode->next());
					}
				}

				pinfos[name] = pi;
			} while (programNode->next());

			delete reader;
			return pinfos;
		}

		map<string, MaterialInformation> SpecificationParser::parseMaterialInformation()
		{
			map<string, MaterialInformation> minfos;

			utils::XmlReader* reader = utils::XmlReader::fromFile(mFilename);

			auto materialNode = reader->getNode("Specification/Materials/Material");
			do
			{
				string name;
				if (!materialNode->getOptionalAttribute("name", name))
				{
					name = "";
				}

				MaterialInformation mi(name);

				// Parse program
				auto programName = materialNode->getChild("Program");
				string program = programName->getAttribute("name");

				mi.setProgram(program);

				minfos[name] = mi;
			} while (materialNode->next());

			delete reader;
			return minfos;
		}

		mpp::mesh::MeshSpecification SpecificationParser::parseMeshSpecification(uint32& maxVerticesPerMesh)
		{
			mpp::mesh::MeshSpecification meshSpec;
			utils::XmlReader* reader = utils::XmlReader::fromFile(mFilename);

			auto buffersNode = reader->getNode("Specification/Buffers");

			// Is indexed?
			string indexed = utils::StringUtils::toLower(buffersNode->getAttribute("indexed"));
			bool isIndexed = indexed == "true" || indexed == "yes";

			if (isIndexed)
			{
				meshSpec.setIndexedVertices(true);
			}

			// Get primitive type
			string primitive = utils::StringUtils::toLower(buffersNode->getAttribute("primitive"));

			if (primitive == "points")
			{
				meshSpec.setPrimitiveType(mpp::mesh::Primitive::Type::Points);
			}
			else if (primitive == "lines")
			{
				meshSpec.setPrimitiveType(mpp::mesh::Primitive::Type::Lines);
			}
			else if (primitive == "triangles")
			{
				meshSpec.setPrimitiveType(mpp::mesh::Primitive::Type::Triangles);
			}
			else
			{
				string errMsg = "Invalid primitive: " + primitive;
				throw exception(errMsg.c_str());
			}

			// Get storage type
			string storage = utils::StringUtils::toLower(buffersNode->getAttribute("storage"));

			if (storage == "static")
			{
				meshSpec.setStorageType(mpp::mesh::VertexBufferStorageType::Static);
			}
			else if (storage == "dynamic")
			{
				meshSpec.setStorageType(mpp::mesh::VertexBufferStorageType::Dynamic);
			}
			else
			{
				string errMsg = "Invalid storage type: " + storage;
				throw exception(errMsg.c_str());
			}

			// Get split size
			string splitSize;
			maxVerticesPerMesh = (uint32)-1;
			if (buffersNode->getOptionalAttribute("splitSize", splitSize))
			{
				maxVerticesPerMesh = utils::StringUtils::parseUInt(splitSize);
			}

			auto bufferNode = buffersNode->getChild("Buffer");
			uint32_t attribIndex = 0;
			if (bufferNode)
			{
				do
				{
					mpp::mesh::VertexBufferAttributeLayout* attribLayout = meshSpec.createVertexBufferAttributeLayout();

					auto channelNode = bufferNode->getChild("Channel");

					int channelId = 0;
					if (channelNode)
					{
						do
						{
							string data = utils::StringUtils::toLower(channelNode->getAttribute("data"));
							string type = utils::StringUtils::toLower(channelNode->getAttribute("type"));

							if (mComponentTypes.find(data) == mComponentTypes.end())
							{
								string errMsg = "Invalid data: " + data;
								throw exception(errMsg.c_str());
							}
							if (mDataTypes.find(type) == mDataTypes.end())
							{
								string errMsg = "Invalid type: " + type;
								throw exception(errMsg.c_str());
							}

							string normalised;
							bool isNormalised;

							if (channelNode->getOptionalAttribute("normalised", normalised))
							{
								utils::StringUtils::toLower(normalised);
								isNormalised = normalised == "true" || normalised == "yes";
							}
							else
							{
								isNormalised = false;
							}

							attribLayout->createAttribute(mComponentTypes[data], mDataTypes[type], isNormalised);
							channelId++;
						} while (channelNode->next());
					}
				} while (bufferNode->next());
			}

			delete reader;
			return meshSpec;
		}
	}
}