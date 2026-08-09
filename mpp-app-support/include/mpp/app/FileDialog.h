#pragma once

#include <optional>
#include <string>

struct SDL_Window;

namespace mpp::app
{
	std::optional<std::string> openXmlFileDialog(SDL_Window* owner, std::string const& title);
	std::optional<std::string> saveXmlFileDialog(SDL_Window* owner, std::string const& title, std::string const& defaultName);
	std::optional<std::string> savePackageFileDialog(SDL_Window* owner, std::string const& title, std::string const& defaultName);
	std::optional<std::string> openExecutableFileDialog(SDL_Window* owner, std::string const& title);
	std::optional<std::string> openImageFileDialog(SDL_Window* owner, std::string const& title);
	std::optional<std::string> openHdrExrFileDialog(SDL_Window* owner, std::string const& title);
	std::optional<std::string> openGltfFileDialog(SDL_Window* owner, std::string const& title);
	std::optional<std::string> selectFolderDialog(SDL_Window* owner, std::string const& title);
}
