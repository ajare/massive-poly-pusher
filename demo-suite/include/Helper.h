#pragma once

#include <string>
#include <map>

#include <mpp/TextureStream.h>
#include <mpp/ResourceManager.h>

#include "mpp/app/WindowSDL.h"

typedef std::map<std::pair<int, int>, std::vector<SDL_DisplayMode>> DisplayModeSet;

mpp::TextureData loadImage(std::string const& filename);

mpp::TextureStream* loadImageAtlas(std::string const& filename);

void loadAllImages(std::string const& dir, bool flipY, mpp::ResourceManager* resourceMgr);

void rationalApproximation(float value, int md, int &num, int &denom);

DisplayModeSet getVideoModes(int displayDevice);
