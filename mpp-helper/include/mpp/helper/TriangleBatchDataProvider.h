#pragma once

#include <mpp/BatchDataProvider.h>
#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>

#include <mpp/mesh/VertexTypeSpecification.h>

#include "Config.h"

namespace mpp
{
	namespace helper
	{

		template<typename PosType, typename TexType, typename ColType = mpp::mesh::DataTypeNone>
		class TriangleBatchDataProvider : public BatchDataProvider
		{
		public:

			virtual void position(uint32_t index, typename PosType::builtin_type& x0, typename PosType::builtin_type& y0,
				typename PosType::builtin_type& x1, typename PosType::builtin_type& y1,
				typename PosType::builtin_type& x2, typename PosType::builtin_type& y2) = 0;

			virtual void texcoords(uint32_t index, typename PosType::builtin_type& u0, typename PosType::builtin_type& v0,
				typename PosType::builtin_type& u1, typename PosType::builtin_type& v1,
				typename PosType::builtin_type& u2, typename PosType::builtin_type& v2) {}

			virtual void colour(uint32_t index, typename ColType::builtin_type& red, typename ColType::builtin_type& green, typename ColType::builtin_type& blue, typename ColType::builtin_type& alpha) = 0;

			virtual mpp::Colour diffuse() = 0;
		};

		template<typename PosType, typename TexType>
		class TriangleBatchDataProvider<PosType, TexType, mpp::mesh::DataTypeNone> : public BatchDataProvider
		{
		public:

			virtual void position(uint32_t index, typename PosType::builtin_type& x0, typename PosType::builtin_type& y0,
				typename PosType::builtin_type& x1, typename PosType::builtin_type& y1,
				typename PosType::builtin_type& x2, typename PosType::builtin_type& y2) = 0;

			virtual void texcoords(uint32_t index, typename PosType::builtin_type& u0, typename PosType::builtin_type& v0,
				typename PosType::builtin_type& u1, typename PosType::builtin_type& v1,
				typename PosType::builtin_type& u2, typename PosType::builtin_type& v2) {}

			virtual mpp::Colour diffuse() = 0;

		};
	}
}