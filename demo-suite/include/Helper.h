#pragma once

#include <string>
#include <map>

#include <freeimage/freeimage.h>

#include <mpp/TextureStream.h>
#include <mpp/ResourceManager.h>

#include "sdl/WindowSDL.h"

typedef std::map<std::pair<int, int>, std::vector<SDL_DisplayMode>> DisplayModeSet;

mpp::TextureData loadImage(std::string const& filename);

mpp::TextureAtlasStream* loadImageAtlas(std::string const& filename, bool flipY, size_t imagesX, size_t imagesY);

void loadAllImages(std::string const& dir, bool flipY, mpp::ResourceManager* resourceMgr);

void rationalApproximation(float value, int md, int &num, int &denom);

DisplayModeSet getVideoModes(int displayDevice);
