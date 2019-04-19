#include "VKRenderingSystem.h"

#include "VKRenderingSystemUtilities.h"
#include "../../component/VKRenderingSystemComponent.h"
#include "../../component/RenderingFrontendSystemComponent.h"

#include "VKOpaquePass.h"

#include "../ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

INNO_PRIVATE_SCOPE VKRenderingSystemNS
{
	EntityID m_entityID;

	VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pCallback)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			return func(instance, pCreateInfo, pAllocator, pCallback);
		}
		else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback, const VkAllocationCallbacks* pAllocator)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, callback, pAllocator);
		}
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "VKRenderingSystem: Validation Layer: " + std::string(pCallbackData->pMessage));
		return VK_FALSE;
	}

	std::vector<const char*> getRequiredExtensions()
	{
#if defined INNO_PLATFORM_WIN
		std::vector<const char*> extensions = { "VK_KHR_surface", "VK_KHR_win32_surface" };
#elif  defined INNO_PLATFORM_MAC
		std::vector<const char*> extensions = { "VK_KHR_surface", "VK_MVK_macos_surface" };
#elif  defined INNO_PLATFORM_LINUX
		std::vector<const char*> extensions = { "VK_KHR_surface", "VK_KHR_xcb_surface" };
#endif

		if (VKRenderingSystemComponent::get().m_enableValidationLayers)
		{
			extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	bool createVkInstance();
	bool createDebugCallback();

	bool createPysicalDevice();
	bool createLogicalDevice();

	bool createTextureSamplers();
	bool createCommandPool();

	bool createSwapChain();
	bool createSwapChainCommandBuffers();
	bool createSyncPrimitives();

	ObjectStatus m_objectStatus = ObjectStatus::SHUTDOWN;

	ThreadSafeUnorderedMap<EntityID, VKMeshDataComponent*> m_meshMap;
	ThreadSafeUnorderedMap<EntityID, MaterialDataComponent*> m_materialMap;
	ThreadSafeUnorderedMap<EntityID, VKTextureDataComponent*> m_textureMap;

	void* m_MeshDataComponentPool;
	void* m_MaterialDataComponentPool;
	void* m_TextureDataComponentPool;

	ThreadSafeQueue<VKMeshDataComponent*> m_uninitializedMDC;
	ThreadSafeQueue<VKTextureDataComponent*> m_uninitializedTDC;

	VKTextureDataComponent* m_iconTemplate_OBJ;
	VKTextureDataComponent* m_iconTemplate_PNG;
	VKTextureDataComponent* m_iconTemplate_SHADER;
	VKTextureDataComponent* m_iconTemplate_UNKNOWN;

	VKTextureDataComponent* m_iconTemplate_DirectionalLight;
	VKTextureDataComponent* m_iconTemplate_PointLight;
	VKTextureDataComponent* m_iconTemplate_SphereLight;

	VKMeshDataComponent* m_unitLineMDC;
	VKMeshDataComponent* m_unitQuadMDC;
	VKMeshDataComponent* m_unitCubeMDC;
	VKMeshDataComponent* m_unitSphereMDC;
	VKMeshDataComponent* m_terrainMDC;

	VKTextureDataComponent* m_basicNormalTDC;
	VKTextureDataComponent* m_basicAlbedoTDC;
	VKTextureDataComponent* m_basicMetallicTDC;
	VKTextureDataComponent* m_basicRoughnessTDC;
	VKTextureDataComponent* m_basicAOTDC;

	std::vector<VkImage> m_swapChainImages;
}

bool VKRenderingSystemNS::createVkInstance()
{
	// check support for validation layer
	if (VKRenderingSystemComponent::get().m_enableValidationLayers && !checkValidationLayerSupport()) {
		m_objectStatus = ObjectStatus::STANDBY;
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "VKRenderingSystem: Validation layers requested, but not available!");
		return false;
	}

	// set Vulkan app info
	VkApplicationInfo l_appInfo = {};
	l_appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	l_appInfo.pApplicationName = g_pCoreSystem->getGameSystem()->getGameName().c_str();
	l_appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 7);
	l_appInfo.pEngineName = "Innocence Engine";
	l_appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 7);
	l_appInfo.apiVersion = VK_API_VERSION_1_0;

	// set Vulkan instance create info with app info
	VkInstanceCreateInfo l_createInfo = {};
	l_createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	l_createInfo.pApplicationInfo = &l_appInfo;

	// set window extension info
	auto extensions = getRequiredExtensions();
	l_createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	l_createInfo.ppEnabledExtensionNames = extensions.data();

	if (VKRenderingSystemComponent::get().m_enableValidationLayers)
	{
		l_createInfo.enabledLayerCount = static_cast<uint32_t>(VKRenderingSystemComponent::get().m_validationLayers.size());
		l_createInfo.ppEnabledLayerNames = VKRenderingSystemComponent::get().m_validationLayers.data();
	}
	else
	{
		l_createInfo.enabledLayerCount = 0;
	}

	// create Vulkan instance
	if (vkCreateInstance(&l_createInfo, nullptr, &VKRenderingSystemComponent::get().m_instance) != VK_SUCCESS)
	{
		m_objectStatus = ObjectStatus::STANDBY;
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "VKRenderingSystem: Failed to create VkInstance!");
		return false;
	}

	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "VKRenderingSystem: VkInstance has been created.");
	return true;
}

bool VKRenderingSystemNS::createDebugCallback()
{
	if (VKRenderingSystemComponent::get().m_enableValidationLayers)
	{
		VkDebugUtilsMessengerCreateInfoEXT l_createInfo = {};
		l_createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		l_createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		l_createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		l_createInfo.pfnUserCallback = debugCallback;

		if (createDebugUtilsMessengerEXT(VKRenderingSystemComponent::get().m_instance, &l_createInfo, nullptr, &VKRenderingSystemComponent::get().m_messengerCallback) != VK_SUCCESS)
		{
			m_objectStatus = ObjectStatus::STANDBY;
			g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "VKRenderingSystem: Failed to create DebugUtilsMessenger!");
			return false;
		}

		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "VKRenderingSystem: Validation Layer has been created.");
		return true;
	}
	else
	{
		return true;
	}
}

bool VKRenderingSystemNS::createPysicalDevice()
{
	// check if there is any suitable physical GPU
	uint32_t l_deviceCount = 0;
	vkEnumeratePhysicalDevices(VKRenderingSystemComponent::get().m_instance, &l_deviceCount, nullptr);

	if (l_deviceCount == 0) {
		m_objectStatus = ObjectStatus::STANDBY;
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "VKRenderingSystem: Failed to find GPUs with Vulkan support!");
		return false;
	}

	// assign the handle
	std::vector<VkPhysicalDevice> l_devices(l_deviceCount);
	vkEnumeratePhysicalDevices(VKRenderingSystemComponent::get().m_instance, &l_deviceCount, l_devices.data());

	for (const auto& device : l_devices)
	{
		if (isDeviceSuitable(device))
		{
			VKRenderingSystemComponent::get().m_physicalDevice = device;
			break;
		}
	}

	if (VKRenderingSystemComponent::get().m_physicalDevice == VK_NULL_HANDLE)
	{
		m_objectStatus = ObjectStatus::STANDBY;
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "VKRenderingSystem: Failed to find a suitable GPU!");
		return false;
	}

	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "VKRenderingSystem: VkPhysicalDevice has been created.");
	return true;
}

bool VKRenderingSystemNS::createLogicalDevice()
{
	QueueFamilyIndices l_indices = findQueueFamilies(VKRenderingSystemComponent::get().m_physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> l_queueCreateInfos;
	std::set<uint32_t> l_uniqueQueueFamilies = { l_indices.m_graphicsFamily.value(), l_indices.m_presentFamily.value() };

	float l_queuePriority = 1.0f;
	for (uint32_t queueFamily : l_uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo l_queueCreateInfo = {};
		l_queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		l_queueCreateInfo.queueFamilyIndex = queueFamily;
		l_queueCreateInfo.queueCount = 1;
		l_queueCreateInfo.pQueuePriorities = &l_queuePriority;
		l_queueCreateInfos.push_back(l_queueCreateInfo);
	}

	VkPhysicalDeviceFeatures l_deviceFeatures = {};
	l_deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo l_createInfo = {};
	l_createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	l_createInfo.queueCreateInfoCount = static_cast<uint32_t>(l_queueCreateInfos.size());
	l_createInfo.pQueueCreateInfos = l_queueCreateInfos.data();

	l_createInfo.pEnabledFeatures = &l_deviceFeatures;

	l_createInfo.enabledExtensionCount = static_cast<uint32_t>(VKRenderingSystemComponent::get().m_deviceExtensions.size());
	l_createInfo.ppEnabledExtensionNames = VKRenderingSystemComponent::get().m_deviceExtensions.data();

	if (VKRenderingSystemComponent::get().m_enableValidationLayers)
	{
		l_createInfo.enabledLayerCount = static_cast<uint32_t>(VKRenderingSystemComponent::get().m_validationLayers.size());
		l_createInfo.ppEnabledLayerNames = VKRenderingSystemComponent::get().m_validationLayers.data();
	}
	else
	{
		l_createInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(VKRenderingSystemComponent::get().m_physicalDevice, &l_createInfo, nullptr, &VKRenderingSystemComponent::get().m_device) != VK_SUCCESS)
	{
		m_objectStatus = ObjectStatus::STANDBY;
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "VKRenderingSystem: Failed to create VkDevice!");
		return false;
	}

	vkGetDeviceQueue(VKRenderingSystemComponent::get().m_device, l_indices.m_graphicsFamily.value(), 0, &VKRenderingSystemComponent::get().m_graphicsQueue);
	vkGetDeviceQueue(VKRenderingSystemComponent::get().m_device, l_indices.m_presentFamily.value(), 0, &VKRenderingSystemComponent::get().m_presentQueue);

	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "VKRenderingSystem: VkDevice has been created.");
	return true;
}

bool VKRenderingSystemNS::createTextureSamplers()
{
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

	if (vkCreateSampler(VKRenderingSystemComponent::get().m_device, &samplerInfo, nullptr, &VKRenderingSystemComponent::get().m_deferredRTSampler) != VK_SUCCESS)
	{
		m_objectStatus = ObjectStatus::STANDBY;
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "VKRenderingSystem: Failed to create VkSampler for deferred pass render target sampling!");
		return false;
	}

	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "VKRenderingSystem: VkSampler for deferred pass render target sampling has been created.");
	return true;
}

bool VKRenderingSystemNS::createCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = findQueueFamilies(VKRenderingSystemComponent::get().m_physicalDevice);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.m_graphicsFamily.value();
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(VKRenderingSystemComponent::get().m_device, &poolInfo, nullptr, &VKRenderingSystemComponent::get().m_commandPool) != VK_SUCCESS)
	{
		m_objectStatus = ObjectStatus::STANDBY;
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "VKRenderingSystem: Failed to create CommandPool!");
		return false;
	}

	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "VKRenderingSystem: CommandPool has been created.");
	return true;
}

bool VKRenderingSystemNS::createSwapChain()
{
	// choose device supported formats, modes and maximum back buffers
	SwapChainSupportDetails l_swapChainSupport = querySwapChainSupport(VKRenderingSystemComponent::get().m_physicalDevice);

	VkSurfaceFormatKHR l_surfaceFormat = chooseSwapSurfaceFormat(l_swapChainSupport.m_formats);
	VkPresentModeKHR l_presentMode = chooseSwapPresentMode(l_swapChainSupport.m_presentModes);
	VkExtent2D l_extent = chooseSwapExtent(l_swapChainSupport.m_capabilities);

	uint32_t l_imageCount = l_swapChainSupport.m_capabilities.minImageCount + 1;
	if (l_swapChainSupport.m_capabilities.maxImageCount > 0 && l_imageCount > l_swapChainSupport.m_capabilities.maxImageCount)
	{
		l_imageCount = l_swapChainSupport.m_capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR l_createInfo = {};
	l_createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	l_createInfo.surface = VKRenderingSystemComponent::get().m_windowSurface;
	l_createInfo.minImageCount = l_imageCount;
	l_createInfo.imageFormat = l_surfaceFormat.format;
	l_createInfo.imageColorSpace = l_surfaceFormat.colorSpace;
	l_createInfo.imageExtent = l_extent;
	l_createInfo.imageArrayLayers = 1;
	l_createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices l_indices = findQueueFamilies(VKRenderingSystemComponent::get().m_physicalDevice);
	uint32_t l_queueFamilyIndices[] = { l_indices.m_graphicsFamily.value(), l_indices.m_presentFamily.value() };

	if (l_indices.m_graphicsFamily.value() != l_indices.m_presentFamily.value())
	{
		l_createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		l_createInfo.queueFamilyIndexCount = 2;
		l_createInfo.pQueueFamilyIndices = l_queueFamilyIndices;
	}
	else
	{
		l_createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	l_createInfo.preTransform = l_swapChainSupport.m_capabilities.currentTransform;
	l_createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	l_createInfo.presentMode = l_presentMode;
	l_createInfo.clipped = VK_TRUE;

	l_createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(VKRenderingSystemComponent::get().m_device, &l_createInfo, nullptr, &VKRenderingSystemComponent::get().m_swapChain) != VK_SUCCESS)
	{
		m_objectStatus = ObjectStatus::STANDBY;
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "VKRenderingSystem: Failed to create VkSwapChainKHR!");
		return false;
	}

	// get swap chain VkImages
	// get count
	vkGetSwapchainImagesKHR(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_swapChain, &l_imageCount, nullptr);

	m_swapChainImages.reserve(l_imageCount);
	for (size_t i = 0; i < l_imageCount; i++)
	{
		m_swapChainImages.emplace_back();
	}
	// get real VkImages
	vkGetSwapchainImagesKHR(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_swapChain, &l_imageCount, m_swapChainImages.data());

	// add shader component
	auto l_VKSPC = addVKShaderProgramComponent(m_entityID);

	ShaderFilePaths l_shaderFilePaths;
	l_shaderFilePaths.m_VSPath = "VK//finalBlendPass.vert.spv";
	l_shaderFilePaths.m_FSPath = "VK//finalBlendPass.frag.spv";

	initializeVKShaderProgramComponent(l_VKSPC, l_shaderFilePaths);

	// add render pass component
	auto l_VKRPC = addVKRenderPassComponent();

	l_VKRPC->m_renderPassDesc = VKRenderingSystemComponent::get().m_deferredRenderPassDesc;
	l_VKRPC->m_renderPassDesc.RTNumber = l_imageCount;

	l_VKRPC->m_renderPassDesc.useMultipleFramebuffers = (l_imageCount > 1);

	VkTextureDataDesc l_VkTextureDataDesc;
	l_VkTextureDataDesc.textureWrapMethod = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	l_VkTextureDataDesc.magFilterParam = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	l_VkTextureDataDesc.minFilterParam = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	l_VkTextureDataDesc.internalFormat = l_surfaceFormat.format;

	// initialize manually
	bool l_result = true;

	l_result &= reserveRenderTargets(l_VKRPC);

	// use device created swap chain VkImages
	for (size_t i = 0; i < l_imageCount; i++)
	{
		l_VKRPC->m_VKTDCs[i]->m_VkTextureDataDesc = l_VkTextureDataDesc;
		l_VKRPC->m_VKTDCs[i]->m_image = m_swapChainImages[i];
		createImageView(l_VKRPC->m_VKTDCs[i]);
	}

	// sub-pass
	l_VKRPC->attachmentRef.attachment = 0;
	l_VKRPC->attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	l_VKRPC->subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	l_VKRPC->subpassDesc.colorAttachmentCount = 1;
	l_VKRPC->subpassDesc.pColorAttachments = &l_VKRPC->attachmentRef;

	// render pass
	VkAttachmentDescription attachmentDesc = {};

	attachmentDesc.format = l_surfaceFormat.format;
	attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	l_VKRPC->attachmentDescs.emplace_back(attachmentDesc);

	l_VKRPC->renderPassCInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	l_VKRPC->renderPassCInfo.attachmentCount = 1;
	l_VKRPC->renderPassCInfo.pAttachments = &l_VKRPC->attachmentDescs[0];
	l_VKRPC->renderPassCInfo.subpassCount = 1;
	l_VKRPC->renderPassCInfo.pSubpasses = &l_VKRPC->subpassDesc;

	// set descriptor pool size info
	VkDescriptorPoolSize basePassRTDescPoolSize;
	basePassRTDescPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	basePassRTDescPoolSize.descriptorCount = 1;
	l_VKRPC->descriptorPoolSizes.emplace_back(basePassRTDescPoolSize);

	// set descriptor set layout binding info
	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	l_VKRPC->descriptorSetLayoutBindings.emplace_back(samplerLayoutBinding);

	// set descriptor image info
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageInfo.imageView = VKOpaquePass::getVKRPC()->m_VKTDCs[0]->m_imageView;
	imageInfo.sampler = VKRenderingSystemComponent::get().m_deferredRTSampler;
	l_VKRPC->descriptorImageInfos.emplace_back(imageInfo);

	VkWriteDescriptorSet basePassRTWriteDescriptorSet = {};
	basePassRTWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	basePassRTWriteDescriptorSet.dstBinding = 1;
	basePassRTWriteDescriptorSet.dstArrayElement = 0;
	basePassRTWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	basePassRTWriteDescriptorSet.descriptorCount = 1;
	basePassRTWriteDescriptorSet.pImageInfo = &l_VKRPC->descriptorImageInfos[0];
	l_VKRPC->writeDescriptorSets.emplace_back(basePassRTWriteDescriptorSet);

	// set pipeline fix stages info
	l_VKRPC->inputAssemblyStateCInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	l_VKRPC->inputAssemblyStateCInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	l_VKRPC->inputAssemblyStateCInfo.primitiveRestartEnable = VK_FALSE;

	l_VKRPC->viewport.x = 0.0f;
	l_VKRPC->viewport.y = 0.0f;
	l_VKRPC->viewport.width = (float)l_extent.width;
	l_VKRPC->viewport.height = (float)l_extent.height;
	l_VKRPC->viewport.minDepth = 0.0f;
	l_VKRPC->viewport.maxDepth = 1.0f;

	l_VKRPC->scissor.offset = { 0, 0 };
	l_VKRPC->scissor.extent = l_extent;

	l_VKRPC->viewportStateCInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	l_VKRPC->viewportStateCInfo.viewportCount = 1;
	l_VKRPC->viewportStateCInfo.pViewports = &l_VKRPC->viewport;
	l_VKRPC->viewportStateCInfo.scissorCount = 1;
	l_VKRPC->viewportStateCInfo.pScissors = &l_VKRPC->scissor;

	l_VKRPC->rasterizationStateCInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	l_VKRPC->rasterizationStateCInfo.depthClampEnable = VK_FALSE;
	l_VKRPC->rasterizationStateCInfo.rasterizerDiscardEnable = VK_FALSE;
	l_VKRPC->rasterizationStateCInfo.polygonMode = VK_POLYGON_MODE_FILL;
	l_VKRPC->rasterizationStateCInfo.lineWidth = 1.0f;
	l_VKRPC->rasterizationStateCInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	l_VKRPC->rasterizationStateCInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	l_VKRPC->rasterizationStateCInfo.depthBiasEnable = VK_FALSE;

	l_VKRPC->multisampleStateCInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	l_VKRPC->multisampleStateCInfo.sampleShadingEnable = VK_FALSE;
	l_VKRPC->multisampleStateCInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	l_VKRPC->colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	l_VKRPC->colorBlendAttachmentState.blendEnable = VK_FALSE;

	l_VKRPC->colorBlendStateCInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	l_VKRPC->colorBlendStateCInfo.logicOpEnable = VK_FALSE;
	l_VKRPC->colorBlendStateCInfo.logicOp = VK_LOGIC_OP_COPY;
	l_VKRPC->colorBlendStateCInfo.attachmentCount = 1;
	l_VKRPC->colorBlendStateCInfo.pAttachments = &l_VKRPC->colorBlendAttachmentState;
	l_VKRPC->colorBlendStateCInfo.blendConstants[0] = 0.0f;
	l_VKRPC->colorBlendStateCInfo.blendConstants[1] = 0.0f;
	l_VKRPC->colorBlendStateCInfo.blendConstants[2] = 0.0f;
	l_VKRPC->colorBlendStateCInfo.blendConstants[3] = 0.0f;

	l_VKRPC->pipelineLayoutCInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	l_VKRPC->pipelineLayoutCInfo.setLayoutCount = 1;

	l_result &= createRenderPass(l_VKRPC);

	if (l_VKRPC->m_renderPassDesc.useMultipleFramebuffers)
	{
		l_result &= createMultipleFramebuffers(l_VKRPC);
	}
	else
	{
		l_result &= createSingleFramebuffer(l_VKRPC);
	}

	l_result &= createDescriptorPool(l_VKRPC);
	l_result &= createDescriptorSetLayout(l_VKRPC);
	l_result &= createDescriptorSet(l_VKRPC);
	l_result &= updateDescriptorSet(l_VKRPC);

	l_result &= createPipelineLayout(l_VKRPC);

	l_result &= createGraphicsPipelines(l_VKRPC, l_VKSPC);

	VKRenderingSystemComponent::get().m_swapChainVKSPC = l_VKSPC;

	VKRenderingSystemComponent::get().m_swapChainVKRPC = l_VKRPC;

	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "VKRenderingSystem: VkSwapChainKHR has been created.");
	return true;
}

bool VKRenderingSystemNS::createSwapChainCommandBuffers()
{
	auto l_result = createCommandBuffers(VKRenderingSystemComponent::get().m_swapChainVKRPC);

	for (size_t i = 0; i < VKRenderingSystemComponent::get().m_swapChainVKRPC->m_commandBuffers.size(); i++)
	{
		recordCommand(VKRenderingSystemComponent::get().m_swapChainVKRPC, (unsigned int)i, [&]() {
			recordDescriptorBinding(VKRenderingSystemComponent::get().m_swapChainVKRPC, (unsigned int)i);
			auto l_MDC = getVKMeshDataComponent(MeshShapeType::QUAD);
			recordDrawCall(VKRenderingSystemComponent::get().m_swapChainVKRPC, (unsigned int)i, l_MDC);
		});
	}

	return l_result;
}

bool VKRenderingSystemNS::createSyncPrimitives()
{
	VKRenderingSystemComponent::get().m_swapChainVKRPC->m_maxFramesInFlight = 2;
	VKRenderingSystemComponent::get().m_imageAvailableSemaphores.resize(VKRenderingSystemComponent::get().m_swapChainVKRPC->m_maxFramesInFlight);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (size_t i = 0; i < VKRenderingSystemComponent::get().m_swapChainVKRPC->m_maxFramesInFlight; i++)
	{
		if (vkCreateSemaphore(
			VKRenderingSystemComponent::get().m_device,
			&semaphoreInfo,
			nullptr,
			&VKRenderingSystemComponent::get().m_imageAvailableSemaphores[i])
			!= VK_SUCCESS)
		{
			g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "VKRenderingSystem: Failed to create swap chain image available semaphores!");
			return false;
		}
	}

	auto l_result = createSyncPrimitives(VKRenderingSystemComponent::get().m_swapChainVKRPC);

	return l_result;
}

bool VKRenderingSystemNS::generateGPUBuffers()
{
	generateUBO(VKRenderingSystemComponent::get().m_cameraUBO, sizeof(CameraGPUData), VKRenderingSystemComponent::get().m_cameraUBOMemory);

	return true;
}

bool VKRenderingSystemNS::setup()
{
	m_entityID = InnoMath::createEntityID();

	initializeComponentPool();

	auto l_screenResolution = g_pCoreSystem->getVisionSystem()->getRenderingFrontend()->getScreenResolution();

	// general render pass desc
	VKRenderingSystemComponent::get().m_deferredRenderPassDesc.RTNumber = 1;
	VKRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.samplerType = TextureSamplerType::SAMPLER_2D;
	VKRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.usageType = TextureUsageType::RENDER_TARGET;
	VKRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.colorComponentsFormat = TextureColorComponentsFormat::RGBA16F;
	VKRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.pixelDataFormat = TexturePixelDataFormat::RGBA;
	VKRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.minFilterMethod = TextureFilterMethod::NEAREST;
	VKRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.magFilterMethod = TextureFilterMethod::NEAREST;
	VKRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.wrapMethod = TextureWrapMethod::CLAMP_TO_EDGE;
	VKRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.width = l_screenResolution.x;
	VKRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.height = l_screenResolution.y;
	VKRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.pixelDataType = TexturePixelDataType::FLOAT;

	bool result = true;
	result = result && createVkInstance();
	result = result && createDebugCallback();

	m_objectStatus = ObjectStatus::ALIVE;
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "VKRenderingSystem setup finished.");
	return result;
}

bool VKRenderingSystemNS::initialize()
{
	m_MeshDataComponentPool = g_pCoreSystem->getMemorySystem()->allocateMemoryPool(sizeof(VKMeshDataComponent), 16384);
	m_MaterialDataComponentPool = g_pCoreSystem->getMemorySystem()->allocateMemoryPool(sizeof(MaterialDataComponent), 32768);
	m_TextureDataComponentPool = g_pCoreSystem->getMemorySystem()->allocateMemoryPool(sizeof(VKTextureDataComponent), 32768);

	bool result = true;

	result = result && createPysicalDevice();
	result = result && createLogicalDevice();

	result = result && createTextureSamplers();
	result = result && createCommandPool();

	loadDefaultAssets();

	generateGPUBuffers();

	VKOpaquePass::initialize();

	result = result && createSwapChain();
	result = result && createSwapChainCommandBuffers();
	result = result && createSyncPrimitives();

	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "VKRenderingSystem has been initialized.");
	return result;
}

void VKRenderingSystemNS::loadDefaultAssets()
{
	auto l_basicNormalTDC = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//basic_normal.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_basicAlbedoTDC = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//basic_albedo.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::ALBEDO);
	auto l_basicMetallicTDC = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//basic_metallic.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::METALLIC);
	auto l_basicRoughnessTDC = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//basic_roughness.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::ROUGHNESS);
	auto l_basicAOTDC = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//basic_ao.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::AMBIENT_OCCLUSION);

	auto l_iconTemplate_OBJ = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//InnoFileTypeIcons_OBJ.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_iconTemplate_PNG = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//InnoFileTypeIcons_PNG.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_iconTemplate_SHADER = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//InnoFileTypeIcons_SHADER.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_iconTemplate_UNKNOWN = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//InnoFileTypeIcons_UNKNOWN.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);

	auto l_iconTemplate_DirectionalLight = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//InnoWorldEditorIcons_DirectionalLight.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_iconTemplate_PointLight = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//InnoWorldEditorIcons_PointLight.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_iconTemplate_SphereLight = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//InnoWorldEditorIcons_SphereLight.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);

	m_basicNormalTDC = reinterpret_cast<VKTextureDataComponent*>(l_basicNormalTDC);
	m_basicAlbedoTDC = reinterpret_cast<VKTextureDataComponent*>(l_basicAlbedoTDC);
	m_basicMetallicTDC = reinterpret_cast<VKTextureDataComponent*>(l_basicMetallicTDC);
	m_basicRoughnessTDC = reinterpret_cast<VKTextureDataComponent*>(l_basicRoughnessTDC);
	m_basicAOTDC = reinterpret_cast<VKTextureDataComponent*>(l_basicAOTDC);

	m_iconTemplate_OBJ = reinterpret_cast<VKTextureDataComponent*>(l_iconTemplate_OBJ);
	m_iconTemplate_PNG = reinterpret_cast<VKTextureDataComponent*>(l_iconTemplate_PNG);
	m_iconTemplate_SHADER = reinterpret_cast<VKTextureDataComponent*>(l_iconTemplate_SHADER);
	m_iconTemplate_UNKNOWN = reinterpret_cast<VKTextureDataComponent*>(l_iconTemplate_UNKNOWN);

	m_iconTemplate_DirectionalLight = reinterpret_cast<VKTextureDataComponent*>(l_iconTemplate_DirectionalLight);
	m_iconTemplate_PointLight = reinterpret_cast<VKTextureDataComponent*>(l_iconTemplate_PointLight);
	m_iconTemplate_SphereLight = reinterpret_cast<VKTextureDataComponent*>(l_iconTemplate_SphereLight);

	m_unitLineMDC = addVKMeshDataComponent();
	g_pCoreSystem->getAssetSystem()->addUnitLine(*m_unitLineMDC);
	m_unitLineMDC->m_meshPrimitiveTopology = MeshPrimitiveTopology::TRIANGLE_STRIP;
	m_unitLineMDC->m_meshShapeType = MeshShapeType::LINE;
	m_unitLineMDC->m_objectStatus = ObjectStatus::STANDBY;
	g_pCoreSystem->getPhysicsSystem()->generatePhysicsDataComponent(m_unitLineMDC);

	m_unitQuadMDC = addVKMeshDataComponent();
	g_pCoreSystem->getAssetSystem()->addUnitQuad(*m_unitQuadMDC);
	// Flip y texture coordinate
	for (auto& i : m_unitQuadMDC->m_vertices)
	{
		i.m_texCoord.y = 1.0f - i.m_texCoord.y;
	}
	m_unitQuadMDC->m_meshPrimitiveTopology = MeshPrimitiveTopology::TRIANGLE;
	m_unitQuadMDC->m_meshShapeType = MeshShapeType::QUAD;
	m_unitQuadMDC->m_objectStatus = ObjectStatus::STANDBY;
	g_pCoreSystem->getPhysicsSystem()->generatePhysicsDataComponent(m_unitQuadMDC);

	m_unitCubeMDC = addVKMeshDataComponent();
	g_pCoreSystem->getAssetSystem()->addUnitCube(*m_unitCubeMDC);
	m_unitCubeMDC->m_meshPrimitiveTopology = MeshPrimitiveTopology::TRIANGLE;
	m_unitCubeMDC->m_meshShapeType = MeshShapeType::CUBE;
	m_unitCubeMDC->m_objectStatus = ObjectStatus::STANDBY;
	g_pCoreSystem->getPhysicsSystem()->generatePhysicsDataComponent(m_unitCubeMDC);

	m_unitSphereMDC = addVKMeshDataComponent();
	g_pCoreSystem->getAssetSystem()->addUnitSphere(*m_unitSphereMDC);
	m_unitSphereMDC->m_meshPrimitiveTopology = MeshPrimitiveTopology::TRIANGLE_STRIP;
	m_unitSphereMDC->m_meshShapeType = MeshShapeType::SPHERE;
	m_unitSphereMDC->m_objectStatus = ObjectStatus::STANDBY;
	g_pCoreSystem->getPhysicsSystem()->generatePhysicsDataComponent(m_unitSphereMDC);

	m_terrainMDC = addVKMeshDataComponent();
	g_pCoreSystem->getAssetSystem()->addTerrain(*m_terrainMDC);
	m_terrainMDC->m_meshPrimitiveTopology = MeshPrimitiveTopology::TRIANGLE;
	m_terrainMDC->m_objectStatus = ObjectStatus::STANDBY;
	g_pCoreSystem->getPhysicsSystem()->generatePhysicsDataComponent(m_terrainMDC);

	initializeVKMeshDataComponent(m_unitLineMDC);
	initializeVKMeshDataComponent(m_unitQuadMDC);
	initializeVKMeshDataComponent(m_unitCubeMDC);
	initializeVKMeshDataComponent(m_unitSphereMDC);
	initializeVKMeshDataComponent(m_terrainMDC);

	initializeVKTextureDataComponent(m_basicNormalTDC);
	initializeVKTextureDataComponent(m_basicAlbedoTDC);
	initializeVKTextureDataComponent(m_basicMetallicTDC);
	initializeVKTextureDataComponent(m_basicRoughnessTDC);
	initializeVKTextureDataComponent(m_basicAOTDC);

	initializeVKTextureDataComponent(m_iconTemplate_OBJ);
	initializeVKTextureDataComponent(m_iconTemplate_PNG);
	initializeVKTextureDataComponent(m_iconTemplate_SHADER);
	initializeVKTextureDataComponent(m_iconTemplate_UNKNOWN);

	initializeVKTextureDataComponent(m_iconTemplate_DirectionalLight);
	initializeVKTextureDataComponent(m_iconTemplate_PointLight);
	initializeVKTextureDataComponent(m_iconTemplate_SphereLight);
}

bool VKRenderingSystemNS::update()
{
	updateUBO(VKRenderingSystemComponent::get().m_cameraUBOMemory, RenderingFrontendSystemComponent::get().m_cameraGPUData);
	VKOpaquePass::update();

	return true;
}

bool VKRenderingSystemNS::render()
{
	VKOpaquePass::render();

	waitForFence(VKRenderingSystemComponent::get().m_swapChainVKRPC);

	// acquire an image from swap chain
	thread_local uint32_t imageIndex;
	vkAcquireNextImageKHR(
		VKRenderingSystemComponent::get().m_device,
		VKRenderingSystemComponent::get().m_swapChain,
		std::numeric_limits<uint64_t>::max(),
		VKRenderingSystemComponent::get().m_imageAvailableSemaphores[VKRenderingSystemComponent::get().m_swapChainVKRPC->m_currentFrame],
		VK_NULL_HANDLE,
		&imageIndex);

	// set swap chain image available wait semaphore
	VkSemaphore l_availableSemaphores[] = {
		VKOpaquePass::getVKRPC()->m_renderFinishedSemaphores[0],
		VKRenderingSystemComponent::get().m_imageAvailableSemaphores[VKRenderingSystemComponent::get().m_swapChainVKRPC->m_currentFrame]
	};

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VKRenderingSystemComponent::get().m_swapChainVKRPC->submitInfo.waitSemaphoreCount = 2;
	VKRenderingSystemComponent::get().m_swapChainVKRPC->submitInfo.pWaitSemaphores = l_availableSemaphores;
	VKRenderingSystemComponent::get().m_swapChainVKRPC->submitInfo.pWaitDstStageMask = waitStages;

	summitCommand(VKRenderingSystemComponent::get().m_swapChainVKRPC, imageIndex);

	// present the swap chain image to the front screen
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	// wait semaphore
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &VKRenderingSystemComponent::get().m_swapChainVKRPC->m_renderFinishedSemaphores[VKRenderingSystemComponent::get().m_swapChainVKRPC->m_currentFrame];

	// swap chain
	VkSwapchainKHR swapChains[] = { VKRenderingSystemComponent::get().m_swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;

	presentInfo.pImageIndices = &imageIndex;

	vkQueuePresentKHR(VKRenderingSystemComponent::get().m_presentQueue, &presentInfo);

	VKRenderingSystemComponent::get().m_swapChainVKRPC->m_currentFrame = (VKRenderingSystemComponent::get().m_swapChainVKRPC->m_currentFrame + 1) % VKRenderingSystemComponent::get().m_swapChainVKRPC->m_maxFramesInFlight;

	return true;
}

bool VKRenderingSystemNS::terminate()
{
	vkDeviceWaitIdle(VKRenderingSystemComponent::get().m_device);

	VKOpaquePass::terminate();

	destroyVKShaderProgramComponent(VKRenderingSystemComponent::get().m_swapChainVKSPC);

	for (size_t i = 0; i < VKRenderingSystemComponent::get().m_swapChainVKRPC->m_maxFramesInFlight; i++)
	{
		vkDestroySemaphore(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_swapChainVKRPC->m_renderFinishedSemaphores[i], nullptr);
		vkDestroyFence(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_swapChainVKRPC->m_inFlightFences[i], nullptr);
	}

	vkFreeCommandBuffers(VKRenderingSystemComponent::get().m_device,
		VKRenderingSystemComponent::get().m_commandPool,
		static_cast<uint32_t>(VKRenderingSystemComponent::get().m_swapChainVKRPC->m_commandBuffers.size()),
		VKRenderingSystemComponent::get().m_swapChainVKRPC->m_commandBuffers.data());

	vkDestroyBuffer(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_cameraUBO, nullptr);
	vkFreeMemory(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_cameraUBOMemory, nullptr);

	vkDestroySampler(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_deferredRTSampler, nullptr);
	vkFreeMemory(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_textureImageMemory, nullptr);

	destroyAllGraphicPrimitiveComponents();

	vkFreeMemory(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_indexBufferMemory, nullptr);
	vkFreeMemory(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_vertexBufferMemory, nullptr);

	vkDestroyPipeline(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_swapChainVKRPC->m_pipeline, nullptr);
	vkDestroyPipelineLayout(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_swapChainVKRPC->m_pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_swapChainVKRPC->descriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_swapChainVKRPC->descriptorPool, nullptr);

	for (auto framebuffer : VKRenderingSystemComponent::get().m_swapChainVKRPC->m_framebuffers)
	{
		vkDestroyFramebuffer(VKRenderingSystemComponent::get().m_device, framebuffer, nullptr);
	}

	vkDestroyRenderPass(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_swapChainVKRPC->m_renderPass, nullptr);

	for (auto VKTDC : VKRenderingSystemComponent::get().m_swapChainVKRPC->m_VKTDCs)
	{
		vkDestroyImageView(VKRenderingSystemComponent::get().m_device, VKTDC->m_imageView, nullptr);
	}

	vkDestroySwapchainKHR(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_swapChain, nullptr);

	vkDestroyCommandPool(VKRenderingSystemComponent::get().m_device, VKRenderingSystemComponent::get().m_commandPool, nullptr);

	vkDestroyDevice(VKRenderingSystemComponent::get().m_device, nullptr);

	if (VKRenderingSystemComponent::get().m_enableValidationLayers)
	{
		destroyDebugUtilsMessengerEXT(VKRenderingSystemComponent::get().m_instance, VKRenderingSystemComponent::get().m_messengerCallback, nullptr);
	}

	vkDestroySurfaceKHR(VKRenderingSystemComponent::get().m_instance, VKRenderingSystemComponent::get().m_windowSurface, nullptr);

	vkDestroyInstance(VKRenderingSystemComponent::get().m_instance, nullptr);

	VKRenderingSystemNS::m_objectStatus = ObjectStatus::SHUTDOWN;
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "VKRenderingSystem has been terminated.");

	return true;
}

VKMeshDataComponent* VKRenderingSystemNS::addVKMeshDataComponent()
{
	auto l_rawPtr = g_pCoreSystem->getMemorySystem()->spawnObject(m_MeshDataComponentPool, sizeof(VKMeshDataComponent));
	auto l_MDC = new(l_rawPtr)VKMeshDataComponent();
	auto l_parentEntity = InnoMath::createEntityID();
	l_MDC->m_parentEntity = l_parentEntity;
	auto l_meshMap = &m_meshMap;
	l_meshMap->emplace(std::pair<EntityID, VKMeshDataComponent*>(l_parentEntity, l_MDC));
	return l_MDC;
}

MaterialDataComponent* VKRenderingSystemNS::addVKMaterialDataComponent()
{
	auto l_rawPtr = g_pCoreSystem->getMemorySystem()->spawnObject(m_MaterialDataComponentPool, sizeof(MaterialDataComponent));
	auto l_MDC = new(l_rawPtr)MaterialDataComponent();
	auto l_parentEntity = InnoMath::createEntityID();
	l_MDC->m_parentEntity = l_parentEntity;
	auto l_materialMap = &m_materialMap;
	l_materialMap->emplace(std::pair<EntityID, MaterialDataComponent*>(l_parentEntity, l_MDC));
	return l_MDC;
}

VKTextureDataComponent* VKRenderingSystemNS::addVKTextureDataComponent()
{
	auto l_rawPtr = g_pCoreSystem->getMemorySystem()->spawnObject(m_TextureDataComponentPool, sizeof(VKTextureDataComponent));
	auto l_TDC = new(l_rawPtr)VKTextureDataComponent();
	auto l_parentEntity = InnoMath::createEntityID();
	l_TDC->m_parentEntity = l_parentEntity;
	auto l_textureMap = &m_textureMap;
	l_textureMap->emplace(std::pair<EntityID, VKTextureDataComponent*>(l_parentEntity, l_TDC));
	return l_TDC;
}

VKMeshDataComponent* VKRenderingSystemNS::getVKMeshDataComponent(EntityID EntityID)
{
	auto result = VKRenderingSystemNS::m_meshMap.find(EntityID);
	if (result != VKRenderingSystemNS::m_meshMap.end())
	{
		return result->second;
	}
	else
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "RenderingBackendSystem: can't find MeshDataComponent by EntityID: " + EntityID + " !");
		return nullptr;
	}
}

VKTextureDataComponent * VKRenderingSystemNS::getVKTextureDataComponent(EntityID EntityID)
{
	auto result = VKRenderingSystemNS::m_textureMap.find(EntityID);
	if (result != VKRenderingSystemNS::m_textureMap.end())
	{
		return result->second;
	}
	else
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "RenderingBackendSystem: can't find TextureDataComponent by EntityID: " + EntityID + " !");
		return nullptr;
	}
}

VKMeshDataComponent* VKRenderingSystemNS::getVKMeshDataComponent(MeshShapeType meshShapeType)
{
	switch (meshShapeType)
	{
	case MeshShapeType::LINE:
		return VKRenderingSystemNS::m_unitLineMDC; break;
	case MeshShapeType::QUAD:
		return VKRenderingSystemNS::m_unitQuadMDC; break;
	case MeshShapeType::CUBE:
		return VKRenderingSystemNS::m_unitCubeMDC; break;
	case MeshShapeType::SPHERE:
		return VKRenderingSystemNS::m_unitSphereMDC; break;
	case MeshShapeType::TERRAIN:
		return VKRenderingSystemNS::m_terrainMDC; break;
	case MeshShapeType::CUSTOM:
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "RenderingBackendSystem: wrong MeshShapeType passed to VKRenderingSystem::getMeshDataComponent() !");
		return nullptr; break;
	default:
		return nullptr; break;
	}
}

VKTextureDataComponent * VKRenderingSystemNS::getVKTextureDataComponent(TextureUsageType textureUsageType)
{
	switch (textureUsageType)
	{
	case TextureUsageType::INVISIBLE:
		return nullptr; break;
	case TextureUsageType::NORMAL:
		return VKRenderingSystemNS::m_basicNormalTDC; break;
	case TextureUsageType::ALBEDO:
		return VKRenderingSystemNS::m_basicAlbedoTDC; break;
	case TextureUsageType::METALLIC:
		return VKRenderingSystemNS::m_basicMetallicTDC; break;
	case TextureUsageType::ROUGHNESS:
		return VKRenderingSystemNS::m_basicRoughnessTDC; break;
	case TextureUsageType::AMBIENT_OCCLUSION:
		return VKRenderingSystemNS::m_basicAOTDC; break;
	case TextureUsageType::RENDER_TARGET:
		return nullptr; break;
	default:
		return nullptr; break;
	}
}

VKTextureDataComponent * VKRenderingSystemNS::getVKTextureDataComponent(FileExplorerIconType iconType)
{
	switch (iconType)
	{
	case FileExplorerIconType::OBJ:
		return VKRenderingSystemNS::m_iconTemplate_OBJ; break;
	case FileExplorerIconType::PNG:
		return VKRenderingSystemNS::m_iconTemplate_PNG; break;
	case FileExplorerIconType::SHADER:
		return VKRenderingSystemNS::m_iconTemplate_SHADER; break;
	case FileExplorerIconType::UNKNOWN:
		return VKRenderingSystemNS::m_iconTemplate_UNKNOWN; break;
	default:
		return nullptr; break;
	}
}

VKTextureDataComponent * VKRenderingSystemNS::getVKTextureDataComponent(WorldEditorIconType iconType)
{
	switch (iconType)
	{
	case WorldEditorIconType::DIRECTIONAL_LIGHT:
		return VKRenderingSystemNS::m_iconTemplate_DirectionalLight; break;
	case WorldEditorIconType::POINT_LIGHT:
		return VKRenderingSystemNS::m_iconTemplate_PointLight; break;
	case WorldEditorIconType::SPHERE_LIGHT:
		return VKRenderingSystemNS::m_iconTemplate_SphereLight; break;
	default:
		return nullptr; break;
	}
}

bool VKRenderingSystemNS::resize()
{
	return true;
}

bool VKRenderingSystem::setup()
{
	return VKRenderingSystemNS::setup();
}

bool VKRenderingSystem::initialize()
{
	return VKRenderingSystemNS::initialize();
}

bool VKRenderingSystem::update()
{
	return VKRenderingSystemNS::update();
}

bool VKRenderingSystem::render()
{
	return VKRenderingSystemNS::render();
}

bool VKRenderingSystem::terminate()
{
	return VKRenderingSystemNS::terminate();
}

ObjectStatus VKRenderingSystem::getStatus()
{
	return VKRenderingSystemNS::m_objectStatus;
}

MeshDataComponent * VKRenderingSystem::addMeshDataComponent()
{
	return VKRenderingSystemNS::addVKMeshDataComponent();
}

MaterialDataComponent * VKRenderingSystem::addMaterialDataComponent()
{
	return VKRenderingSystemNS::addVKMaterialDataComponent();
}

TextureDataComponent * VKRenderingSystem::addTextureDataComponent()
{
	return VKRenderingSystemNS::addVKTextureDataComponent();
}

MeshDataComponent * VKRenderingSystem::getMeshDataComponent(EntityID meshID)
{
	return VKRenderingSystemNS::getVKMeshDataComponent(meshID);
}

TextureDataComponent * VKRenderingSystem::getTextureDataComponent(EntityID textureID)
{
	return VKRenderingSystemNS::getVKTextureDataComponent(textureID);
}

MeshDataComponent * VKRenderingSystem::getMeshDataComponent(MeshShapeType MeshShapeType)
{
	return VKRenderingSystemNS::getVKMeshDataComponent(MeshShapeType);
}

TextureDataComponent * VKRenderingSystem::getTextureDataComponent(TextureUsageType TextureUsageType)
{
	return VKRenderingSystemNS::getVKTextureDataComponent(TextureUsageType);
}

TextureDataComponent * VKRenderingSystem::getTextureDataComponent(FileExplorerIconType iconType)
{
	return VKRenderingSystemNS::getVKTextureDataComponent(iconType);
}

TextureDataComponent * VKRenderingSystem::getTextureDataComponent(WorldEditorIconType iconType)
{
	return VKRenderingSystemNS::getVKTextureDataComponent(iconType);
}

bool VKRenderingSystem::removeMeshDataComponent(EntityID EntityID)
{
	auto l_meshMap = &VKRenderingSystemNS::m_meshMap;
	auto l_mesh = l_meshMap->find(EntityID);
	if (l_mesh != l_meshMap->end())
	{
		g_pCoreSystem->getMemorySystem()->destroyObject(VKRenderingSystemNS::m_MeshDataComponentPool, sizeof(VKMeshDataComponent), l_mesh->second);
		l_meshMap->erase(EntityID);
		return true;
	}
	else
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "RenderingBackendSystem: can't remove MeshDataComponent by EntityID: " + EntityID + " !");
		return false;
	}
}

bool VKRenderingSystem::removeTextureDataComponent(EntityID EntityID)
{
	auto l_textureMap = &VKRenderingSystemNS::m_textureMap;
	auto l_texture = l_textureMap->find(EntityID);
	if (l_texture != l_textureMap->end())
	{
		for (auto& i : l_texture->second->m_textureData)
		{
			// @TODO
		}

		g_pCoreSystem->getMemorySystem()->destroyObject(VKRenderingSystemNS::m_TextureDataComponentPool, sizeof(VKTextureDataComponent), l_texture->second);
		l_textureMap->erase(EntityID);
		return true;
	}
	else
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "RenderingBackendSystem: can't remove TextureDataComponent by EntityID: " + EntityID + " !");
		return false;
	}
}

void VKRenderingSystem::registerUninitializedMeshDataComponent(MeshDataComponent * rhs)
{
	VKRenderingSystemNS::m_uninitializedMDC.push(reinterpret_cast<VKMeshDataComponent*>(rhs));
}

void VKRenderingSystem::registerUninitializedTextureDataComponent(TextureDataComponent * rhs)
{
	VKRenderingSystemNS::m_uninitializedTDC.push(reinterpret_cast<VKTextureDataComponent*>(rhs));
}

bool VKRenderingSystem::resize()
{
	return VKRenderingSystemNS::resize();
}

bool VKRenderingSystem::reloadShader(RenderPassType renderPassType)
{
	switch (renderPassType)
	{
	case RenderPassType::Shadow:
		break;
	case RenderPassType::Opaque:
		break;
	case RenderPassType::Light:
		break;
	case RenderPassType::Transparent:
		break;
	case RenderPassType::Terrain:
		break;
	case RenderPassType::PostProcessing:
		break;
	default: break;
	}

	return true;
}

bool VKRenderingSystem::bakeGI()
{
	return true;
}