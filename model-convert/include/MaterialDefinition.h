#pragma once

#include <string>
#include <map>

struct MaterialDefinition
{
	std::string name;
	std::string program;

	std::map<std::string, std::string> textureBindings;
	std::map<std::string, std::string> uniformValues;
};