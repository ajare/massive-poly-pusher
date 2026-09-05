#pragma once

#include <string>
#include <vector>

#include <mpp/Diagnostic.h>

namespace particle_editor
{
	class DiagnosticsView
	{
		mpp::DiagnosticBag mDocumentDiagnostics;
		std::string mPreviewFailure;
		std::vector<std::string> mPreviewWarnings;
		std::string mOperationFailure;

	public:
		void setDocumentDiagnostics(mpp::DiagnosticBag const& diagnostics);
		void setPreviewFailure(std::string failure);
		void setPreviewWarnings(std::vector<std::string> warnings);
		void setOperationFailure(std::string failure);
		bool hasErrors() const;
		size_t errorCount() const;
		std::string statusText() const;
		void draw(bool* open);
	};
}
