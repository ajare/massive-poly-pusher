#include <chrono>
#include <fstream>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include "mpp/app/DocumentFile.h"

using namespace std;

namespace mpp::app
{
	filesystem::path normaliseDocumentPath(filesystem::path const& path)
	{
		if (path.empty()) return {};
		error_code error;
		auto absolute = filesystem::absolute(path, error);
		return (error ? path : absolute).lexically_normal();
	}

	filesystem::path resolveDocumentReference(filesystem::path const& document, filesystem::path const& reference)
	{
		if (reference.empty()) return {};
		if (reference.is_absolute()) return normaliseDocumentPath(reference);
		return normaliseDocumentPath(document.parent_path() / reference);
	}

	filesystem::path makeDocumentRelativeReference(filesystem::path const& document, filesystem::path const& target)
	{
		auto base = normaliseDocumentPath(document).parent_path();
		auto absoluteTarget = normaliseDocumentPath(target);
		error_code error;
		auto relative = filesystem::relative(absoluteTarget, base, error);
		return error || relative.empty() ? absoluteTarget : relative.lexically_normal();
	}

	void atomicWriteText(filesystem::path const& destination, string const& text)
	{
		if (destination.empty()) throw invalid_argument("Atomic write destination is empty.");
		auto normalised = normaliseDocumentPath(destination);
		if (!normalised.parent_path().empty()) filesystem::create_directories(normalised.parent_path());

		auto nonce = chrono::steady_clock::now().time_since_epoch().count();
		auto temporary = normalised;
		temporary += ".tmp." + to_string(nonce);
		try
		{
			ofstream output(temporary, ios::binary | ios::trunc);
			if (!output.is_open()) throw runtime_error("Could not open temporary document for writing: " + temporary.string());
			output.write(text.data(), static_cast<streamsize>(text.size()));
			output.flush();
			if (!output.good()) throw runtime_error("Could not write temporary document: " + temporary.string());
			output.close();

#ifdef _WIN32
			if (!MoveFileExW(temporary.c_str(), normalised.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
				throw system_error(static_cast<int>(GetLastError()), system_category(), "Could not replace document atomically");
#else
			filesystem::rename(temporary, normalised);
#endif
		}
		catch (...)
		{
			error_code ignored;
			filesystem::remove(temporary, ignored);
			throw;
		}
	}
}
