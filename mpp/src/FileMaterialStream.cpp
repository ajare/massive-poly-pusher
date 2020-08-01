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
	FileMaterialStream::FileMaterialStream(FileDataStream const& dataStream)
		: MaterialStream()
	{
		mXmlDefinition = string((char const*)dataStream.getData(), dataStream.getDataSize());
	}

	/*
	 * Constructor.
	 *
	 */
	FileMaterialStream::FileMaterialStream(string const& xmlDef)
		: MaterialStream()
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

		// Get program
		auto programNode = materialNode->getChild("Program");
		mProgram = programNode->getAttribute("name");

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
					mTextures[binding] = resource;
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