#pragma once

#include <memory>

#include "mpp/Config.h"

namespace mpp
{
	class BatchRenderer
	{
	public:

		virtual void create() = 0;

		virtual size_t update(size_t count) { return count; }

		virtual void render() = 0;
	};

	typedef std::shared_ptr<BatchRenderer> BatchRendererPtr;
}