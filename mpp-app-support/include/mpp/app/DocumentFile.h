#pragma once

#include <filesystem>
#include <string>

namespace mpp::app
{
	std::filesystem::path normaliseDocumentPath(std::filesystem::path const& path);

	std::filesystem::path resolveDocumentReference(
		std::filesystem::path const& document,
		std::filesystem::path const& reference);

	std::filesystem::path makeDocumentRelativeReference(
		std::filesystem::path const& document,
		std::filesystem::path const& target);

	void atomicWriteText(std::filesystem::path const& destination, std::string const& text);
}
