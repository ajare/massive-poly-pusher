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
			enum class ShaderStage
			{
				Vertex,
				Geometry,
				Fragment,
				NumStages,
			};

		private:

			std::string mName;

			std::string mSources[(int)ShaderStage::NumStages];

			mesh::MeshSpecification mSpecification;

		private:

			void parseAttributeUsage(ShaderStage stage);

		public:

			Parser();

			explicit Parser(std::string const& name);

			void setVertexSource(std::string const& src);

			void setFragmentSource(std::string const& src);

			void setMeshSpecification(mesh::MeshSpecification const& spec);

			void build();
		};

	}
}