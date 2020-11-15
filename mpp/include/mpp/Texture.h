#pragma once

#include "mpp/Resource.h"
#include "mpp/TextureParams.h"
#include "mpp/Sampler.h"

namespace mpp
{
	class _MPPAPI Texture : public Resource
	{
		TextureParams mParams;

		size_t mWidth, mHeight, mDepth, mBitsPerPixel;

		uint32_t mPixelFormat, mDataType;

		uint32_t mTarget;

		uint32_t mInternalFormat;

		uint32_t mSortId;

		ResourcePtr mSampler;

	protected:

		void createImpl();

		void destroyImpl();

		void loadImpl();

		void unloadImpl();

	public:

		Texture(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		virtual int getWidth() const;

		virtual int getHeight() const;

		virtual int getDepth() const;

		virtual int getBitsPerPixel() const;

		virtual void bind(uint32_t unit);

		void setSortId(uint32_t sortId);

		uint32_t getSortId() const;
	};

}
