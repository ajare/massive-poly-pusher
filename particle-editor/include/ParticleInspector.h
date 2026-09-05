#pragma once

#include <cstddef>
#include <optional>

namespace particle_editor
{
	class ParticleDocument;
	class ParticleResourceLibrary;

	class ParticleInspector
	{
		int mSelectedScalarCurve{ 0 };
		std::optional<size_t> mSelectedScalarKey;
		std::optional<size_t> mSelectedGradientKey;
		size_t mEditedEmitter{ size_t(-1) };

	public:
		void draw(ParticleDocument& document, ParticleResourceLibrary const& resources);
	};
}
