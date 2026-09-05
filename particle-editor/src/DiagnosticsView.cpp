#include "DiagnosticsView.h"

#include <utility>

#include <imgui/imgui.h>

namespace particle_editor
{
	void DiagnosticsView::setDocumentDiagnostics(mpp::DiagnosticBag const& diagnostics)
	{
		mDocumentDiagnostics = diagnostics;
	}

	void DiagnosticsView::setPreviewFailure(std::string failure)
	{
		mPreviewFailure = std::move(failure);
	}

	void DiagnosticsView::setPreviewWarnings(std::vector<std::string> warnings)
	{
		mPreviewWarnings = std::move(warnings);
	}

	void DiagnosticsView::setOperationFailure(std::string failure)
	{
		mOperationFailure = std::move(failure);
	}

	bool DiagnosticsView::hasErrors() const
	{
		return mDocumentDiagnostics.hasErrors() || !mPreviewFailure.empty() || !mOperationFailure.empty();
	}

	size_t DiagnosticsView::errorCount() const
	{
		return mDocumentDiagnostics.count(mpp::DiagnosticSeverity::Error) +
			(!mPreviewFailure.empty() ? 1u : 0u) + (!mOperationFailure.empty() ? 1u : 0u);
	}

	std::string DiagnosticsView::statusText() const
	{
		if (!mOperationFailure.empty()) return mOperationFailure;
		if (!mPreviewFailure.empty()) return mPreviewFailure;
		for (auto const& diagnostic : mDocumentDiagnostics.getDiagnostics())
			if (diagnostic.severity == mpp::DiagnosticSeverity::Error)
				return "[" + diagnostic.code + "] " + diagnostic.message;
		if (!mPreviewWarnings.empty()) return "Preview warning: " + mPreviewWarnings.front();
		return "No errors";
	}

	void DiagnosticsView::draw(bool* open)
	{
		if (!ImGui::Begin("Diagnostics", open))
		{
			ImGui::End();
			return;
		}
		if (!mOperationFailure.empty())
			ImGui::TextWrapped("Operation: %s", mOperationFailure.c_str());
		if (!mPreviewFailure.empty())
			ImGui::TextWrapped("Preview: %s", mPreviewFailure.c_str());
		for (auto const& warning : mPreviewWarnings)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.25f, 1.0f));
			ImGui::TextUnformatted("[PREVIEW-INPUT] warning");
			ImGui::PopStyleColor();
			ImGui::TextWrapped("%s", warning.c_str());
			ImGui::Separator();
		}
		for (auto const& diagnostic : mDocumentDiagnostics.getDiagnostics())
		{
			auto colour = diagnostic.severity == mpp::DiagnosticSeverity::Error ? ImVec4(1.0f, 0.35f, 0.3f, 1.0f) :
				diagnostic.severity == mpp::DiagnosticSeverity::Warning ? ImVec4(1.0f, 0.75f, 0.25f, 1.0f) :
				ImVec4(0.65f, 0.8f, 1.0f, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Text, colour);
			ImGui::Text("[%s] %s", diagnostic.code.c_str(), mpp::diagnosticSeverityName(diagnostic.severity));
			ImGui::PopStyleColor();
			ImGui::TextWrapped("%s", diagnostic.message.c_str());
			if (!diagnostic.location.elementPath.empty())
				ImGui::TextDisabled("%s", diagnostic.location.elementPath.c_str());
			ImGui::Separator();
		}
		if (!hasErrors() && mDocumentDiagnostics.empty() && mPreviewWarnings.empty()) ImGui::TextDisabled("No diagnostics.");
		ImGui::End();
	}
}
