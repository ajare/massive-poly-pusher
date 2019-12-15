#pragma once

#include <string>
#include <map>

class MaterialInformation
{
	std::string mName;

	std::string mProgram;

public:

	MaterialInformation() {}

	explicit MaterialInformation(std::string const& name);

	std::string const& getName() const;

	void setProgram(std::string const& program);

	std::string const& getProgram() const;
};