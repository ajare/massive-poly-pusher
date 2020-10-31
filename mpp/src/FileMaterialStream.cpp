#include <stack>

#include "utils/StringUtils.h"
#include "utils/XmlReader.h"

#include "mpp/FileMaterialStream.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	FileMaterialStream::FileMaterialStream(ResourceManager* resourceMgr, FileDataStream const& dataStream)
		: MaterialStream(resourceMgr)
	{
		mXmlDefinition = string((char const*)dataStream.getData(), dataStream.getDataSize());
	}

	/*
	 * Constructor.
	 *
	 */
	FileMaterialStream::FileMaterialStream(ResourceManager* resourceMgr, string const& xmlDef)
		: MaterialStream(resourceMgr)
		, mXmlDefinition(xmlDef)
	{
	}

	/*
	 * Load from file.
	 *
	 */
	void FileMaterialStream::loadImpl()
	{
		utils::XmlReader* reader = utils::XmlReader::fromString(mXmlDefinition);

		auto materialNodes = reader->getNode("Materials");
		auto materialNode = materialNodes->getChild("Material");

		// Get name
		mName = materialNode->getAttribute("name");

		// Get shaders
		auto shadersNode = materialNode->getChild("Shaders");

		// Vertex shader
		auto vertexNode = shadersNode->getOptionalChild("Vertex");
		if (!vertexNode)
		{
			string errMsg = utils::StringUtils::format(
				"No vertex shader specified  while parsing material '{}'.", mName);

			THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
		}

		string vertexShader;
		if (!vertexNode->getOptionalAttribute("name", vertexShader))
		{
			vertexShader = "";
		}

		// Fragment shader
		auto fragmentNode = shadersNode->getOptionalChild("Fragment");
		if (!fragmentNode)
		{
			string errMsg = utils::StringUtils::format(
				"No fragment shader specified  while parsing material '{}'.", mName);

			THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
		}

		string fragmentShader;
		if (!fragmentNode->getOptionalAttribute("name", fragmentShader))
		{
			fragmentShader = "";
		}

		// Get textures
		auto texturesNode = materialNode->getOptionalChild("Textures");
		if (texturesNode)
		{
			auto textureNode = texturesNode->getOptionalChild("Texture");
			if (textureNode)
			{
				do
				{
					string binding = textureNode->getAttribute("binding");
					string resource = textureNode->getAttribute("resource");
					mTextures[binding] = make_pair(resource, false);
				} while (textureNode->next());
			}
		}

		// Get uniforms
		auto uniformsNode = materialNode->getOptionalChild("Uniforms");
		if (uniformsNode)
		{
			auto uniformNode = uniformsNode->getOptionalChild("Uniform");
			if (uniformNode)
			{
				do
				{
					string uniformName = uniformNode->getAttribute("name");
					string uniformType = uniformNode->getAttribute("type");
					string uniformValue = uniformNode->getAttribute("value");

					if (uniformType != "int" && uniformType != "uint" && uniformType != "float")
					{
						string errMsg = utils::StringUtils::format(
							"Found {}-type uniform '{}' while parsing material '{}'.  Only int/uint/float types are supported.",
							uniformType, uniformName, mName);

						THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
					}

					// Parse value: from [1, 5) components
					vector<string> tokens = utils::StringUtils::split(uniformValue, ",");

					size_t componentCount = tokens.size();

					if (componentCount < 1 || componentCount > 4)
					{
						string errMsg = utils::StringUtils::format(
							"Found {}-dimension uniform '{}' while parsing material '{}'.  Only 1-4 dimensional types are supported.",
							componentCount, uniformName, mName);

						THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
					}

					if (uniformType == "int")
					{
						int32 values[4];
						for (size_t i = 0; i < componentCount; ++i)
						{
							values[i] = utils::StringUtils::parseInt(tokens[i]);
						}

						mUniforms.setUniform(uniformName, componentCount, values);
					}
					else if (uniformType == "uint")
					{
						uint32 values[4];
						for (size_t i = 0; i < componentCount; ++i)
						{
							values[i] = utils::StringUtils::parseUInt(tokens[i]);
						}

						mUniforms.setUniform(uniformName, componentCount, values);
					}
					else if (uniformType == "float")
					{
						float values[4];
						for (size_t i = 0; i < componentCount; ++i)
						{
							values[i] = utils::StringUtils::parseFloat(tokens[i]);
						}

						mUniforms.setUniform(uniformName, componentCount, values);
					}
				} while (uniformNode->next());
			}
		}
	}
}