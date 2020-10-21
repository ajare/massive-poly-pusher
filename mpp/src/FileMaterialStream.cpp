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
					string uniformValue = uniformNode->getAttribute("value");

					// Parse value: from [1, 5) components
					vector<string> tokens = utils::StringUtils::split(uniformValue, ",");

					int componentCount = tokens.size();

					if (componentCount < 1 || componentCount > 4)
					{
						string errMsg = utils::StringUtils::format(
							"Found {}-dimension uniform '{}' while parsing material '{}'.  Only 1-4 dimensional float are supported.",
							componentCount, uniformName, mName);

						THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
					}

					Uniform<float> u;
					u.valueCount = componentCount;
					for (int i = 0; i < componentCount; ++i)
					{
						u.values[i] = utils::StringUtils::parseFloat(tokens[i]);
					}

					mFloatUniforms[uniformName] = u;
				} while (uniformNode->next());
			}
		}
	}
}