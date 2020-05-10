#pragma once

#include <initializer_list>

#include "mpp/PrimitiveModelStream.h"

namespace mpp
{
	class _MPPAPI TiledQuadModelStream : public PrimitiveModelStream
	{
	public:

		TiledQuadModelStream(mesh::MeshSpecification const& meshSpec, std::string const& material, float width, float depth, int dimX, int dimZ);
	};
}
