#include <cstring>
#include <format>
#include <limits>
#include <memory>
#include <stdexcept>

#if defined(_WIN32)
#	include <Windows.h>
#endif

#include <GL/glew.h>
#include <GL/gl.h>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "utils/FileSystem.h"
#include "utils/StringUtils.h"

#include <mpp/BasicMaterialStream.h>
#include <mpp/ProgrammaticTextureStream.h>

#include "Helper.h"
#include "Logger.h"

extern Logger* gLogger;
extern mpp::ResourceManager* gResourceManager;

using namespace std;

mpp::TextureData loadImage(string const& filename)
{
	stbi_set_flip_vertically_on_load_thread(0);

	int width = 0;
	int height = 0;
	int channels = 0;
	size_t componentSize = 0;
	uint32_t glType = 0;
	void* imageData = nullptr;

	if (stbi_is_hdr(filename.c_str()))
	{
		componentSize = sizeof(float);
		glType = GL_FLOAT;
		imageData = stbi_loadf(filename.c_str(), &width, &height, &channels, 0);
	}
	else if (stbi_is_16_bit(filename.c_str()))
	{
		componentSize = sizeof(stbi_us);
		glType = GL_UNSIGNED_SHORT;
		imageData = stbi_load_16(filename.c_str(), &width, &height, &channels, 0);
	}
	else
	{
		componentSize = sizeof(stbi_uc);
		glType = GL_UNSIGNED_BYTE;
		imageData = stbi_load(filename.c_str(), &width, &height, &channels, 0);
	}

	unique_ptr<void, void(*)(void*)> loadedImage(imageData, stbi_image_free);
	if (!loadedImage)
	{
		auto const reason = stbi_failure_reason();
		throw runtime_error("Couldn't open '" + filename + "': " +
			(reason ? reason : "unknown STB image error"));
	}

	if (width <= 0 || height <= 0 || channels < 1 || channels > 4)
	{
		throw runtime_error("Couldn't open '" + filename + "': unsupported image dimensions or channel count.");
	}

	auto const dataWidth = static_cast<size_t>(width);
	auto const dataHeight = static_cast<size_t>(height);
	auto const numChannels = static_cast<size_t>(channels);
	if (dataWidth > numeric_limits<size_t>::max() / dataHeight ||
		dataWidth * dataHeight > numeric_limits<size_t>::max() / numChannels ||
		dataWidth * dataHeight * numChannels > numeric_limits<size_t>::max() / componentSize)
	{
		throw overflow_error("Image data is too large: '" + filename + "'.");
	}

	auto const dataSize = dataWidth * dataHeight * numChannels * componentSize;
	auto textureBytes = make_unique<uint8_t[]>(dataSize);
	memcpy(textureBytes.get(), loadedImage.get(), dataSize);

	uint32_t pixelFormat = 0;
	switch (channels)
	{
	case 1: pixelFormat = GL_RED; break;
	case 2: pixelFormat = GL_RG; break;
	case 3: pixelFormat = GL_RGB; break;
	case 4: pixelFormat = GL_RGBA; break;
	}

	return {
		textureBytes.release(),
		dataWidth,
		dataHeight,
		numChannels * componentSize * 8,
		pixelFormat,
		glType
	};
}

void loadAllImages(string const& dir, bool flipY, mpp::ResourceManager* resourceMgr)
{
	auto files = utils::FileSystem::getFilesInDirectory(utils::FileSystem::DirectoryInfo(dir), "*.png|*.tga", true);
	for (auto const& file: files)
	{
		string const& filePath = file.getFilePath();

		string imageName = filePath.substr(dir.length());
		utils::FileSystem::standardisePath(imageName);

		auto textureData = loadImage(filePath);
		auto tStr = new mpp::ProgrammaticTextureStream(resourceMgr);
		tStr->setTarget(mpp::TextureTarget::Texture2D);
		tStr->setFile(filePath, loadImage);
		tStr->setFiltering(mpp::TextureParams::MinFilter::Linear, mpp::TextureParams::MagFilter::Linear);

		mpp::ResourcePtr tex = resourceMgr->declareResource(imageName, mpp::ResourceStreamPtr(tStr)).first;
		tex->load();
	}
}

void rationalApproximation(float value, int md, int &num, int &denom)
{
	int a, h[3] = { 0, 1, 0 }, k[3] = { 1, 0, 0 };
	int x, d, n = 1;
	int i, neg = 0;

	if (md <= 1)
	{
		denom = 1;
		num = (int)value;
		return;
	}

	if (value < 0)
	{
		neg = 1;
		value = -value;
	}

	while (value != floor(value))
	{
		n <<= 1;
		value *= 2;
	}

	d = (int)value;

	for (i = 0; i < 64; i++)
	{
		a = n ? d / n : 0;
		if (i && !a) break;

		x = d; d = n; n = x % n;

		x = a;
		if (k[1] * a + k[0] >= md) {
			x = (md - k[0]) / k[1];
			if (x * 2 >= a || k[1] >= md)
				i = 65;
			else
				break;
		}

		h[2] = x * h[1] + h[0]; h[0] = h[1]; h[1] = h[2];
		k[2] = x * k[1] + k[0]; k[0] = k[1]; k[1] = k[2];
	}

	denom = k[1];
	num = neg ? -h[1] : h[1];
}

DisplayModeSet getVideoModes(int displayDevice)
{
	// SDL3 identifies displays by ID rather than index, so map the requested
	// index onto the current display list.
	int numDisplays = 0;
	SDL_DisplayID* displays = SDL_GetDisplays(&numDisplays);
	if (!displays || displayDevice < 0 || displayDevice >= numDisplays)
	{
		SDL_free(displays);
		throw runtime_error("Could not find the requested display.");
	}

	SDL_DisplayID displayId = displays[displayDevice];
	SDL_free(displays);

	int numVideoModes = 0;
	SDL_DisplayMode** modeList = SDL_GetFullscreenDisplayModes(displayId, &numVideoModes);
	if (!modeList || numVideoModes == 0)
	{
		SDL_free(modeList);
		throw runtime_error("Could not find any video modes for the default display.");
	}

	vector<SDL_DisplayMode> displayModes;
	for (int i = 0; i < numVideoModes; ++i)
	{
		displayModes.push_back(*modeList[i]);
	}
	SDL_free(modeList);

	// Go through each mode, and work out its aspect ratio.
	DisplayModeSet sortedModes;
	for_each(displayModes.begin(), displayModes.end(), [&sortedModes](SDL_DisplayMode const& displayMode)
	{
		int numer, denom;
		rationalApproximation(displayMode.w / (float)displayMode.h, 10000, numer, denom);

		pair<int, int> aspectRatio(numer, denom);
		if (sortedModes.find(aspectRatio) == sortedModes.end())
		{
			sortedModes[aspectRatio] = vector<SDL_DisplayMode>();
		}

		vector<SDL_DisplayMode>& modes = sortedModes[aspectRatio];
		modes.push_back(displayMode);
	});

	for_each(sortedModes.begin(), sortedModes.end(), [](pair<pair<int, int>, vector<SDL_DisplayMode>> modes)
	{
		gLogger->message(std::format("Display modes at {}x{}", modes.first.first, modes.first.second));

		for_each(modes.second.begin(), modes.second.end(), [](SDL_DisplayMode const& mode)
		{
			gLogger->message(std::format("{}x{} @ {}Hz", mode.w, mode.h, mode.refresh_rate));
		});
	});

	return sortedModes;
}
