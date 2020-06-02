#pragma once

#include <string>

#include <mpp/mesh/MeshSpecification.h>

#include "Config.h"

namespace mpp
{
	namespace program
	{

		class _MPPPROGRAMAPI Parser
		{
			std::string mVertexSource, mFragmentSource;

			mesh::MeshSpecification mSpecification;

		public:

			Parser();

			void setVertexSource(std::string const& src);

			void setFragmentSource(std::string const& src);

			void setMeshSpecification(mesh::MeshSpecification const& spec);

			void build();
		};

	}
}