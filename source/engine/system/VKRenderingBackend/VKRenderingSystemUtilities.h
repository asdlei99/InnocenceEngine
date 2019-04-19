#pragma once
#include "../../common/InnoType.h"

#include "../../component/MeshDataComponent.h"
#include "../../component/MaterialDataComponent.h"
#include "../../component/TextureDataComponent.h"
#include "../../component/VKMeshDataComponent.h"
#include "../../component/VKTextureDataComponent.h"
#include "../../component/VKRenderPassComponent.h"
#include "../../component/VKShaderProgramComponent.h"

struct QueueFamilyIndices
{
	std::optional<uint32_t> m_graphicsFamily;
	std::optional<uint32_t> m_presentFamily;

	bool isComplete()
	{
		return m_graphicsFamily.has_value() && m_presentFamily.has_value();
	}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR m_capabilities;
	std::vector<VkSurfaceFormatKHR> m_formats;
	std::vector<VkPresentModeKHR> m_presentModes;
};

INNO_PRIVATE_SCOPE VKRenderingSystemNS
{
	bool setup();
	bool initialize();
	bool update();
	bool render();
	bool terminate();
	bool initializeComponentPool();
	bool resize();

	void loadDefaultAssets();
	bool generateGPUBuffers();

	VKMeshDataComponent* addVKMeshDataComponent();
	MaterialDataComponent* addVKMaterialDataComponent();
	VKTextureDataComponent* addVKTextureDataComponent();

	VKMeshDataComponent* getVKMeshDataComponent(EntityID meshID);
	VKTextureDataComponent* getVKTextureDataComponent(EntityID textureID);

	VKMeshDataComponent* getVKMeshDataComponent(MeshShapeType MeshShapeType);
	VKTextureDataComponent* getVKTextureDataComponent(TextureUsageType TextureUsageType);
	VKTextureDataComponent* getVKTextureDataComponent(FileExplorerIconType iconType);
	VKTextureDataComponent* getVKTextureDataComponent(WorldEditorIconType iconType);

	bool checkValidationLayerSupport();
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	bool isDeviceSuitable(VkPhysicalDevice device);

	VKRenderPassComponent* addVKRenderPassComponent();
	bool initializeVKRenderPassComponent(VKRenderPassComponent* VKRPC, VKShaderProgramComponent* VKSPC);

	bool reserveRenderTargets(VKRenderPassComponent* VKRPC);
	bool createRenderTargets(VKRenderPassComponent* VKRPC);
	bool createSingleFramebuffer(VKRenderPassComponent* VKRPC);
	bool createMultipleFramebuffers(VKRenderPassComponent* VKRPC);
	bool createRenderPass(VKRenderPassComponent* VKRPC);
	bool createDescriptorSetLayout(VKRenderPassComponent* VKRPC);
	bool createDescriptorSet(VKRenderPassComponent* VKRPC);
	bool updateDescriptorSet(VKRenderPassComponent* VKRPC);
	bool createPipelineLayout(VKRenderPassComponent* VKRPC);
	bool createGraphicsPipelines(VKRenderPassComponent* VKRPC, VKShaderProgramComponent* VKSPC);
	bool createCommandBuffers(VKRenderPassComponent* VKRPC);
	bool createSyncPrimitives(VKRenderPassComponent* VKRPC);

	bool destroyVKRenderPassComponent(VKRenderPassComponent* VKRPC);

	bool initializeVKMeshDataComponent(VKMeshDataComponent* rhs);
	bool initializeVKTextureDataComponent(VKTextureDataComponent* rhs);
	bool createImageView(VKTextureDataComponent* VKTDC);

	bool recordCommand(VKRenderPassComponent* VKRPC, unsigned int commandBufferIndex, const std::function<void()>& commands);

	bool recordDescriptorBinding(VKRenderPassComponent* VKRPC, unsigned int commandBufferIndex);
	bool recordDrawCall(VKRenderPassComponent* VKRPC, unsigned int commandBufferIndex, VKMeshDataComponent * VKMDC);

	bool waitForFence(VKRenderPassComponent* VKRPC);
	bool summitCommand(VKRenderPassComponent* VKRPC, unsigned int commandBufferIndex);

	bool updateUBOImpl(VkDeviceMemory&  UBOMemory, size_t size, const void* UBOValue);

	template<typename T>
	bool updateUBO(VkDeviceMemory&  UBOMemory, const T& UBOValue)
	{
		return updateUBOImpl(UBOMemory, sizeof(T), &UBOValue);
	}

	VkTextureDataDesc getVKTextureDataDesc(const TextureDataDesc & textureDataDesc);

	VKShaderProgramComponent* addVKShaderProgramComponent(const EntityID& rhs);

	bool initializeVKShaderProgramComponent(VKShaderProgramComponent* rhs, const ShaderFilePaths& shaderFilePaths);

	bool generateUBO(VkBuffer& UBO, VkDeviceSize UBOSize, VkDeviceMemory& UBOMemory);
}