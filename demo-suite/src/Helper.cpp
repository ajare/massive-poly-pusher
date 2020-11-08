#include "utils/FileSystem.h"
#include "utils/StringUtils.h"

#include <mpp/MaterialStream.h>
#include <mpp/TextureAtlasStream.h>

#include "Helper.h"
#include "Logger.h"

extern Logger* gLogger;
extern mpp::ResourceManager* gResourceManager;

using namespace std;

void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message)
{
	string errMsg;
	if (fif != FIF_UNKNOWN)
		errMsg += string(FreeImage_GetFormatFromFIF(fif)) + "file: ";

	errMsg += message;
	gLogger->message(errMsg);
}

mpp::TextureData loadImage(string const& filename)
{
	FIBITMAP* bitmap = FreeImage_Load(FreeImage_GetFIFFromFilename(filename.c_str()), filename.c_str());
	if (bitmap)
	{
		int dataWidth = FreeImage_GetWidth(bitmap);
		int dataHeight = FreeImage_GetHeight(bitmap);
		int dataBPP = FreeImage_GetBPP(bitmap);
		int dataSpan = dataWidth * dataBPP / 8;

		int dataSize = dataSpan * dataHeight;
		auto tempData = new uint8[dataSize];

		// Flip vertically?
		int y0, y1, inc;
		
		y0 = 0;
		y1 = dataHeight;
		inc = 1;

		uint8* ptr = (uint8*)FreeImage_GetBits(bitmap);
		for (int y = y0; y != y1; y += inc)
		{
			memcpy(&tempData[y * dataSpan], ptr, dataSpan);
			ptr += dataSpan;
		}

		FreeImage_Unload(bitmap);

		mpp::TextureData textureData
		{
			tempData,
			dataWidth,
			dataHeight,
			dataBPP
		};

		return textureData;
	}
	else
	{
		string errMsg = "Couldn't open '" + filename + "'.";
		throw exception(errMsg.c_str());
	}
}

mpp::TextureAtlasStream* loadImageAtlas(string const& filename, bool flipY, size_t imagesX, size_t imagesY)
{
	FIBITMAP* bitmap = FreeImage_Load(FreeImage_GetFIFFromFilename(filename.c_str()), filename.c_str());
	if (bitmap)
	{
		int dataWidth = FreeImage_GetWidth(bitmap);
		int dataHeight = FreeImage_GetHeight(bitmap);
		int dataBPP = FreeImage_GetBPP(bitmap);
		int dataSpan = dataWidth * dataBPP / 8;

		int dataSize = dataSpan * dataHeight;
		unsigned char* tempData = new unsigned char[dataSize];

		// Flip vertically?
		int y0, y1, inc;
		if (flipY)
		{
			y0 = dataHeight - 1;
			y1 = -1;
			inc = -1;
		}
		else
		{
			y0 = 0;
			y1 = dataHeight;
			inc = 1;
		}

		unsigned char* ptr = (unsigned char*)FreeImage_GetBits(bitmap);
		for (int y = y0; y != y1; y += inc)
		{
			memcpy(&tempData[y * dataSpan], ptr, dataSpan);
			ptr += dataSpan;
		}

		mpp::TextureAtlasStream* tStr{ nullptr };

		tStr = new mpp::TextureAtlasStream(gResourceManager, tempData, dataWidth, dataHeight, dataBPP, true);

		FreeImage_Unload(bitmap);
		delete[] tempData;
		return tStr;
	}
	else
	{
		string errMsg = "Couldn't open '" + filename + "'.";
		throw exception(errMsg.c_str());
	}
}

void loadAllImages(string const& dir, bool flipY, mpp::ResourceManager* resourceMgr)
{
	auto files = utils::FileSystem::getFilesInDirectory(utils::FileSystem::DirectoryInfo(dir), "*.jpg|*.png|*.tga", true);
	for (auto const& file: files)
	{
		string const& filePath = file.getFilePath();

		string imageName = filePath.substr(dir.length());
		utils::FileSystem::standardisePath(imageName);

		auto textureData = loadImage(filePath);
		auto tStr = new mpp::TextureStream(resourceMgr, filePath, loadImage, true);

		mpp::ResourcePtr tex = resourceMgr->declareResource(imageName, mpp::ResourceStreamPtr(tStr));
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
	int numVideoModes = SDL_GetNumDisplayModes(displayDevice);
	if (numVideoModes == 0)
	{
		throw exception("Could not find any video modes for the default display.");
	}

	vector<SDL_DisplayMode> displayModes;
	for (int i = 0; i < numVideoModes; ++i)
	{
		SDL_DisplayMode displayMode = { SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };
		if (SDL_GetDisplayMode(displayDevice, i, &displayMode) == 0)
		{
			displayModes.push_back(displayMode);
		}
	}

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
		gLogger->message(utils::StringUtils::format("Display modes at {}x{}", modes.first.first, modes.first.second));

		for_each(modes.second.begin(), modes.second.end(), [](SDL_DisplayMode const& mode)
		{
			gLogger->message(utils::StringUtils::format("{}x{} @ {}Hz", mode.w, mode.h, mode.refresh_rate));
		});
	});

	return sortedModes;
}
