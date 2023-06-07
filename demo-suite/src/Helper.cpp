#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#	include <Windows.h>
#endif

#include <glew/glew.h>
#include <gl/GL.h>

#include "utils/FileSystem.h"
#include "utils/StringUtils.h"

#include <mpp/MaterialStream.h>
#include <mpp/ProgrammaticTextureStream.h>
#include <mpp/ProgrammaticTextureAtlasStream.h>

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

bool isBigEndian()
{
	union 
	{
		uint32_t i;
		char c[4];
	} bint{ 0x01020304 };

	return bint.c[0] == 1;
}

mpp::TextureData loadImage(string const& filename)
{
	FIBITMAP* bitmap = FreeImage_Load(FreeImage_GetFIFFromFilename(filename.c_str()), filename.c_str());
	if (bitmap)
	{
		auto imageType = FreeImage_GetImageType(bitmap);
		size_t dataWidth = (size_t)FreeImage_GetWidth(bitmap);
		size_t dataHeight = (size_t)FreeImage_GetHeight(bitmap);
		size_t dataBPP = (size_t)FreeImage_GetBPP(bitmap);

		// Calculate data size etc from type and bpp
		size_t typeSize;
		GLuint glType;
		switch (imageType)
		{
		case FIT_BITMAP:
			typeSize = sizeof(uint8_t);
			glType = GL_UNSIGNED_BYTE;
			break;

		case FIT_UINT16:
		case FIT_RGB16:
		case FIT_RGBA16:
			typeSize = sizeof(uint16_t);
			glType = GL_UNSIGNED_SHORT;
			break;

		case FIT_INT16:
			typeSize = sizeof(int16_t);
			glType = GL_SHORT;
			break;

		case FIT_UINT32:
			typeSize = sizeof(uint32_t);
			glType = GL_UNSIGNED_INT;
			break;

		case FIT_INT32:
			typeSize = sizeof(int32_t);
			glType = GL_INT;
			break;

		case FIT_FLOAT:
		case FIT_RGBF:
		case FIT_RGBAF:
			typeSize = sizeof(float);
			glType = GL_FLOAT;
			break;

		case FIT_DOUBLE:
			typeSize = sizeof(double);
			glType = GL_DOUBLE;
			break;

		case FIT_UNKNOWN:
		default:
			string errMsg = "Couldn't open '" + filename + "'.  Unknown/unsupported image type.";
			throw exception(errMsg.c_str());
		}

		size_t dataSpan = dataWidth * dataBPP / 8;
		size_t dataSize = dataSpan * dataHeight * typeSize;
		auto tempData = new uint8_t[dataSize];

		// Flip vertically?
		int y0, y1, inc;
		
		y0 = 0;
		y1 = dataHeight;
		inc = 1;

		uint8_t* ptr = (uint8_t*)FreeImage_GetBits(bitmap);
		for (int y = y0; y != y1; y += inc)
		{
			memcpy(&tempData[y * dataSpan], ptr, dataSpan);
			ptr += dataSpan;
		}

		FreeImage_Unload(bitmap);

		// Calculate pixel format
		size_t numChannels = dataBPP / (8 * typeSize);
		
		uint32_t pixelFormat;
		switch (numChannels)
		{
		case 1:
			pixelFormat = GL_RED;
			break;

		case 2:
			pixelFormat = GL_RG;
			break;

		case 3:
			pixelFormat = GL_BGR;
			break;

		case 4:
			pixelFormat = GL_BGRA;
			break;

		default:
			string errMsg = "Couldn't open '" + filename + "'.  Unknown/unsupported channel count.";
			throw exception(errMsg.c_str());
		}

		if (isBigEndian())
		{
			switch (numChannels)
			{
			case 3:
				pixelFormat = GL_RGB;
				break;

			case 4:
				pixelFormat = GL_RGBA;
				break;

			default:
				break;
			}
		}
		
		mpp::TextureData textureData
		{
			tempData,
			dataWidth,
			dataHeight,
			dataBPP,
			pixelFormat,
			glType
		};

		return textureData;
	}
	else
	{
		string errMsg = "Couldn't open '" + filename + "'.";
		throw exception(errMsg.c_str());
	}
}

mpp::TextureStream* loadImageAtlas(string const& filename)
{
	auto tStr = new mpp::ProgrammaticTextureAtlasStream(gResourceManager);
	tStr->setTarget(mpp::TextureTarget::Texture2D);
	tStr->setData([filename](std::string const& f)
	{
		return loadImage(filename);
	});

	tStr->setFiltering(mpp::TextureParams::MinFilter::Linear, mpp::TextureParams::MagFilter::Linear);
	return tStr;
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
