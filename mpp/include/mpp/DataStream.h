#pragma once

#include "mpp/Config.h"

namespace mpp
{
	class _MPPAPI DataStream
	{
	public:

		virtual int getDataSize() const = 0;

		virtual int8 const* getData() const = 0;
	};
}