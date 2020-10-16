#pragma once

#include <initializer_list>
#include <map>

#include "mpp/PrimitiveModelStream.h"

namespace mpp
{
	class _MPPAPI SphereModelStream : public PrimitiveModelStream
	{
		float mRadius;
		
		int mResolution;

	private:

		void subdivide(int vertexStride);

		uint32 getMidpointIndex(std::map<uint64, uint32>& midpointIndices, std::vector<float>& vertexData, int vertexStride, uint32 i0, uint32 i1);

		void renormalise(int vertexStride);

		void getUvCoord(float nx, float ny, float nz, float* u, float* v);

	public:

		SphereModelStream(ResourceManager* resourceMgr, mesh::MeshSpecification const& meshSpec, std::string const& material, float radius, int res);
	};
}
