#pragma once

#include <optional>
#include <string>

struct SDL_Window;

namespace mpp::app
{
	std::optional<std::string> openXmlFileDialog(SDL_Window* owner, std::string const& title);
	std::optional<std::string> saveXmlFileDialog(SDL_Window* owner, std::string const& title, std::string const& defaultName);
}
