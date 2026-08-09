#include <glew/glew.h>
#include <freeimage/FreeImage.h>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "mpp/app/ImageLoader.h"
namespace mpp::app
{
	TextureData loadImageFile(std::string const& filename)
	{
		auto format=FreeImage_GetFIFFromFilename(filename.c_str());FIBITMAP* bitmap=FreeImage_Load(format,filename.c_str());if(!bitmap)throw std::runtime_error("Could not load image '"+filename+"'.");if(FreeImage_GetImageType(bitmap)==FIT_BITMAP&&(FreeImage_GetColorType(bitmap)==FIC_PALETTE||FreeImage_GetBPP(bitmap)<24)){auto converted=FreeImage_ConvertTo32Bits(bitmap);FreeImage_Unload(bitmap);bitmap=converted;if(!bitmap)throw std::runtime_error("Could not convert indexed image '"+filename+"' to RGBA.");}auto type=FreeImage_GetImageType(bitmap);size_t typeSize=1;uint32_t dataType=GL_UNSIGNED_BYTE;switch(type){case FIT_BITMAP:break;case FIT_UINT16:case FIT_RGB16:case FIT_RGBA16:typeSize=2;dataType=GL_UNSIGNED_SHORT;break;case FIT_INT16:typeSize=2;dataType=GL_SHORT;break;case FIT_UINT32:typeSize=4;dataType=GL_UNSIGNED_INT;break;case FIT_INT32:typeSize=4;dataType=GL_INT;break;case FIT_FLOAT:case FIT_RGBF:case FIT_RGBAF:typeSize=4;dataType=GL_FLOAT;break;case FIT_DOUBLE:typeSize=8;dataType=GL_DOUBLE;break;default:FreeImage_Unload(bitmap);throw std::runtime_error("Unsupported image type in '"+filename+"'.");}size_t width=FreeImage_GetWidth(bitmap),height=FreeImage_GetHeight(bitmap),bits=FreeImage_GetBPP(bitmap),span=width*bits/8,total=span*height;auto pixels=new uint8_t[total];auto source=(uint8_t*)FreeImage_GetBits(bitmap);auto pitch=FreeImage_GetPitch(bitmap);for(size_t y=0;y<height;++y)memcpy(pixels+y*span,source+y*pitch,span);FreeImage_Unload(bitmap);size_t channels=bits/(8*typeSize);bool const floating=type==FIT_FLOAT||type==FIT_RGBF||type==FIT_RGBAF;uint32_t pixelFormat=channels==1?GL_RED:channels==2?GL_RG:channels==3?(floating?GL_RGB:GL_BGR):channels==4?(floating?GL_RGBA:GL_BGRA):0;if(!pixelFormat){delete[] pixels;throw std::runtime_error("Unsupported channel count in '"+filename+"'.");}return {pixels,width,height,bits,pixelFormat,dataType};
	}
}
