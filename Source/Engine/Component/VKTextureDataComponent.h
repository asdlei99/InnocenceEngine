#pragma once
#include "../Common/InnoType.h"
#include "vulkan/vulkan.h"
#include "TextureDataComponent.h"

struct VKTextureDesc
{
	VkImageType imageType;
	VkImageViewType imageViewType;
	VkImageUsageFlags imageUsageFlags;
	VkFormat format;
	VkDeviceSize imageSize;
	VkBorderColor boarderColor;
	VkImageAspectFlags aspectFlags;
};

class VKTextureDataComponent : public TextureDataComponent
{
public:
	VkImage m_image;
	VkImageView m_imageView;
	VKTextureDesc m_VKTextureDesc = {};
	VkImageCreateInfo m_ImageCreateInfo = {};
	VkDescriptorImageInfo m_DescriptorImageInfo = {};
};
