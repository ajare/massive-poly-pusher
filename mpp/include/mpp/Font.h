#pragma once

#include "mpp/Config.h"
#include "mpp/Texture.h"

namespace mpp
{
	class _MPPAPI Font
	{
	public:

		struct Glyph
		{
			float u0_, v0_;
			float u1_, v1_;

			int x, y;
			int width, height;
			int kern, raise;
		};

	private:

		Glyph mGlyphs[256];

		ResourcePtr mTexture;

	private:

		// Methods to be used by RenderSystem
		friend class RenderSystem;

		void setTexture(ResourcePtr texture);

	public:

		Font(ResourcePtr texture);

		void setGlyph(uint8 index, int x, int y, int width, int height, int kern, int raise);

		Glyph const& getGlyph(uint8 index);

		ResourcePtr getTexture();
	};
}