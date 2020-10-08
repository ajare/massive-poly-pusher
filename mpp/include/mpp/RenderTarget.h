#pragma once

#include <memory>

#include "mpp/Config.h"

namespace mpp
{
	class RenderSystem;

	class _MPPAPI RenderTarget
	{
		friend class RenderSystem;

		int mWidth, mHeight;

	private:

		virtual void deactivate() = 0;

		virtual void activate() = 0;
		
	public:

		RenderTarget(int width, int height);

		virtual int getWidth() const;

		virtual int getHeight() const;
	};

	typedef std::shared_ptr<RenderTarget> RenderTargetPtr;
}