#pragma once

#include <initializer_list>

#include "mpp/PrimitiveModelStream.h"

namespace mpp
{
	class _MPPAPI GridModelStream : public PrimitiveModelStream
	{
	public:

		GridModelStream(mesh::MeshSpecification const& meshSpec, std::string const& material, double width, double depth, int dimX, int dimZ);
	};
}
