#pragma once

#include <string>

#include <mpp/Diagnostic.h>

namespace particle_editor
{
	class DiagnosticsView
	{
		mpp::DiagnosticBag mDocumentDiagnostics;
		std::string mPreviewFailure;
		std::string mOperationFailure;

	public:
		void setDocumentDiagnostics(mpp::DiagnosticBag const& diagnostics);
		void setPreviewFailure(std::string failure);
		void setOperationFailure(std::string failure);
		bool hasErrors() const;
		size_t errorCount() const;
		std::string statusText() const;
		void draw(bool* open);
	};
}
