#pragma once

#include <memory>

#include "mpp/Config.h"

namespace mpp
{
	class RenderSystem;

	class _MPPAPI RenderTarget
	{
		friend class RenderSystem;

	protected:

		int mWidth, mHeight;

	private:

		virtual void deactivate() = 0;

		virtual void activate() = 0;
		
	public:

		RenderTarget(int width, int height);

		virtual int getWidth() const;

		virtual int getHeight() const;

		void setViewport(int x, int y, size_t width, size_t height);

		void resetViewport();
	};

	typedef std::shared_ptr<RenderTarget> RenderTargetPtr;
}