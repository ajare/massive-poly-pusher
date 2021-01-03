#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>

#include <mpp/mesh/VertexTypeSpecification.h>

#include "Config.h"

namespace mpp
{
	namespace helper
	{

		template<typename PosType, typename TexType, typename ColType = mpp::mesh::DataTypeNone>
		class TriangleBatchDataProvider
		{
			size_t mNumTriangles{ 0 };

		public:

			virtual void position(uint32_t index, typename PosType::builtin_type& x0, typename PosType::builtin_type& y0,
				typename PosType::builtin_type& x1, typename PosType::builtin_type& y1,
				typename PosType::builtin_type& x2, typename PosType::builtin_type& y2) = 0;

			virtual void texcoords(uint32_t index, typename PosType::builtin_type& u0, typename PosType::builtin_type& v0,
				typename PosType::builtin_type& u1, typename PosType::builtin_type& v1,
				typename PosType::builtin_type& u2, typename PosType::builtin_type& v2) {}

			virtual void colour(uint32_t index, typename ColType::builtin_type& red, typename ColType::builtin_type& green, typename ColType::builtin_type& blue, typename ColType::builtin_type& alpha) = 0;

			virtual mpp::Colour diffuse() = 0;

			void setNumTriangles(size_t numTriangles)
			{
				mNumTriangles = numTriangles;
			}

			size_t getNumTriangles() const
			{
				return mNumTriangles;
			}

			virtual void update(float frameTime) {}
		};

		template<typename PosType, typename TexType>
		class TriangleBatchDataProvider<PosType, TexType, mpp::mesh::DataTypeNone>
		{
			size_t mNumTriangles{ 0 };

		public:

			virtual void position(uint32_t index, typename PosType::builtin_type& x0, typename PosType::builtin_type& y0,
				typename PosType::builtin_type& x1, typename PosType::builtin_type& y1,
				typename PosType::builtin_type& x2, typename PosType::builtin_type& y2) = 0;

			virtual void texcoords(uint32_t index, typename PosType::builtin_type& u0, typename PosType::builtin_type& v0,
				typename PosType::builtin_type& u1, typename PosType::builtin_type& v1,
				typename PosType::builtin_type& u2, typename PosType::builtin_type& v2) {}

			virtual mpp::Colour diffuse() = 0;

			void setNumTriangles(size_t numTriangles)
			{
				mNumTriangles = numTriangles;
			}

			size_t getNumTriangles() const
			{
				return mNumTriangles;
			}

			virtual void update(float frameTime) {}
		};
	}
}