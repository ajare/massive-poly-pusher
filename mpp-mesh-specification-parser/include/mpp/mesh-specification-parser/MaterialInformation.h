#pragma once

#include <string>
#include <map>

#include "mpp/mesh-specification-parser/Config.h"

namespace mpp
{
	namespace mesh_specification_parser
	{
		class _MPPMESHSPECIFICATIONPARSERAPI MaterialInformation
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
	}
}