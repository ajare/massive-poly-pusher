#include <stack>

#include "utils/StringUtils.h"
#include "utils/XmlReader.h"

#include "ProgramOptions.h"

using namespace std;

ProgramOptions parseProgramOptions(string const& filename)
{
	utils::XmlReader* reader = utils::XmlReader::fromFile(filename);

	ProgramOptions pOpts;

	auto videoNode = reader->getNode("Configuration/Video");

	pOpts.screenWidth = utils::StringUtils::parseInt(videoNode->getChild("Width")->getValue());
	pOpts.screenHeight = utils::StringUtils::parseInt(videoNode->getChild("Height")->getValue());

	string fullScreenStr = videoNode->getChild("Fullscreen")->getValue();
	string vsyncStr = videoNode->getChild("VSync")->getValue();

	pOpts.fullScreen = fullScreenStr == "true" || fullScreenStr == "yes";
	pOpts.vSync = vsyncStr == "true" || fullScreenStr == "yes";

	// Resources
	pOpts.resourceLocation = reader->getNode("Configuration/Resources/Location")->getValue();

	delete reader;
	return pOpts;
}