#include "TextureIO.h"

#include "../Core/InnoLogger.h"

#include "../Core/IOService.h"

#include "../ModuleManager/IModuleManager.h"
extern IModuleManager* g_pModuleManager;

#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

TextureDataComponent* InnoFileSystemNS::TextureIO::loadTexture(const std::string & fileName)
{
	int32_t width, height, nrChannels;

	// load image, flip texture
	stbi_set_flip_vertically_on_load(true);

	void* l_rawData;
	auto l_fullPath = IOService::getWorkingDirectory() + fileName;
	auto l_isHDR = stbi_is_hdr(l_fullPath.c_str());

	if (l_isHDR)
	{
		l_rawData = stbi_loadf(l_fullPath.c_str(), &width, &height, &nrChannels, 0);
	}
	else
	{
		l_rawData = stbi_load(l_fullPath.c_str(), &width, &height, &nrChannels, 0);
	}
	if (l_rawData)
	{
		auto l_TDC = g_pModuleManager->getRenderingFrontend()->addTextureDataComponent();

		l_TDC->m_ComponentName = (fileName + "/").c_str();

		l_TDC->m_textureDataDesc.PixelDataFormat = TexturePixelDataFormat(nrChannels - 1);
		l_TDC->m_textureDataDesc.WrapMethod = TextureWrapMethod::Repeat;
		l_TDC->m_textureDataDesc.MinFilterMethod = TextureFilterMethod::Linear;
		l_TDC->m_textureDataDesc.MagFilterMethod = TextureFilterMethod::Linear;
		l_TDC->m_textureDataDesc.PixelDataType = l_isHDR ? TexturePixelDataType::FLOAT16 : TexturePixelDataType::UBYTE;
		l_TDC->m_textureDataDesc.UseMipMap = true;
		l_TDC->m_textureDataDesc.Width = width;
		l_TDC->m_textureDataDesc.Height = height;
		l_TDC->m_textureData = l_rawData;
		l_TDC->m_ObjectStatus = ObjectStatus::Created;

		InnoLogger::Log(LogLevel::Verbose, "FileSystem: TextureIO: STB_Image: ", l_fullPath.c_str(), " has been loaded.");

		return l_TDC;
	}
	else
	{
		InnoLogger::Log(LogLevel::Error, "FileSystem: TextureIO: STB_Image: Failed to load texture: ", l_fullPath.c_str());

		return nullptr;
	}
}

bool InnoFileSystemNS::TextureIO::saveTexture(const std::string & fileName, TextureDataComponent * TDC)
{
	if (TDC->m_textureDataDesc.PixelDataType == TexturePixelDataType::FLOAT16 || TDC->m_textureDataDesc.PixelDataType == TexturePixelDataType::FLOAT32)
	{
		stbi_write_hdr((IOService::getWorkingDirectory() + fileName + ".hdr").c_str(), (int32_t)TDC->m_textureDataDesc.Width, (int32_t)TDC->m_textureDataDesc.Height, (int32_t)TDC->m_textureDataDesc.PixelDataFormat + 1, (float*)TDC->m_textureData);
	}
	else
	{
		stbi_write_png((IOService::getWorkingDirectory() + fileName + ".png").c_str(), (int32_t)TDC->m_textureDataDesc.Width, (int32_t)TDC->m_textureDataDesc.Height, (int32_t)TDC->m_textureDataDesc.PixelDataFormat + 1, TDC->m_textureData, (int32_t)TDC->m_textureDataDesc.Width * sizeof(int32_t));
	}

	return true;
}