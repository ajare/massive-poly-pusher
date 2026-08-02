#pragma once

#include <vector>

#include "mpp/Resource.h"
#include "mpp/TextureParams.h"
#include "mpp/Sampler.h"

namespace mpp
{
	class _MPPAPI Texture : public Resource
	{
		bool mIsAtlas;

	protected:

		TextureParams mParams;

		size_t mWidth, mHeight, mDepth, mBitsPerPixel;

		uint32_t mPixelFormat, mDataType;

		uint32_t mTarget;

		uint32_t mInternalFormat;

		uint32_t mSortId;

		ResourcePtr mSampler;

		size_t mNumAttachments;

		std::vector<uint32_t> mTextureIds;

	protected:

		void createImpl();

		void destroyImpl();

		void loadImpl();

		void unloadImpl();

	public:

		Texture(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		~Texture();

		bool isAtlas() const;

		virtual size_t getWidth() const;

		virtual size_t getHeight() const;

		virtual size_t getDepth() const;

		virtual size_t getBitsPerPixel() const;

		size_t getNumAttachments() const;

		size_t uploadData(int attachment, uint8_t const* data, float u0, float v0, float u1, float v1);

		size_t uploadData(int attachment, uint8_t const* data, uint32_t x, uint32_t y, size_t w, size_t h);

		size_t uploadData(int attachment, uint8_t const* data);

		void uploadCubeFace(uint32_t face, uint8_t const* data);

		virtual void bind(uint32_t unit, uint32_t attachment = 0);

		void setSortId(uint32_t sortId);

		uint32_t getSortId() const;

		int getIdCount() const override;

		int getLiveIdCount() const override;
	};

}
