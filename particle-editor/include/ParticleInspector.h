#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>

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
		std::optional<size_t> mEmitterPendingRemoval;
		std::string mDiagnosticFocusPath;

	public:
		void focusDiagnostic(std::string elementPath) { mDiagnosticFocusPath = std::move(elementPath); }
		void draw(ParticleDocument& document, ParticleResourceLibrary const& resources,
			std::function<void(std::filesystem::path const&)> const& openDocument = {});
	};
}
