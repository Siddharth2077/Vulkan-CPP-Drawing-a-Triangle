#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <algorithm>
#include <optional>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <vector>
#include <set>

// Forward declarations
struct QueueFamilyIndices;
struct SwapChainSupportDetails;

// APPLICATION CLASS
class Application {
public:
	void run();

private:
	// Members:
	GLFWwindow* window;
	const uint32_t WIDTH{ 800 };
	const uint32_t HEIGHT{ 600 };
	const char* APPLICATION_NAME = "Vulkan Application";

	VkInstance vulkanInstance = VK_NULL_HANDLE;
	const int MAX_FRAMES_IN_FLIGHT{ 2 };
	uint32_t currentFrame{ 0 };
	VkPhysicalDevice vulkanPhysicalDevice = VK_NULL_HANDLE;
	VkDevice vulkanLogicalDevice = VK_NULL_HANDLE;
	VkQueue deviceGraphicsQueue = VK_NULL_HANDLE;
	VkQueue devicePresentationQueue = VK_NULL_HANDLE;
	VkSurfaceKHR vulkanSurface = VK_NULL_HANDLE;
	VkSwapchainKHR vulkanSwapChain = VK_NULL_HANDLE;
	VkFormat vulkanSwapChainImageFormat;
	VkColorSpaceKHR vulkanSwapChainImageColorspace;
	VkExtent2D vulkanSwapChainExtent;
	VkRenderPass vulkanRenderPass = VK_NULL_HANDLE;
	VkPipelineLayout vulkanPipelineLayout = VK_NULL_HANDLE;
	VkPipeline vulkanGraphicsPipeline = VK_NULL_HANDLE;
	VkCommandPool vulkanCommandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> vulkanCommandBuffers;
	std::vector<VkImage> vulkanSwapChainImages;
	std::vector<VkImageView> vulkanSwapChainImageViews;
	std::vector<VkFramebuffer> vulkanSwapChainFramebuffers;
	// Synchronization objects:
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector <VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	bool frameBufferResized{ false };
	// Validation layers are now common for instance and devices:
	const std::vector<const char*> vulkanValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};
	// List of physical device extensions to check/enable:
	const std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

#ifdef NDEBUG 
	// Release Mode:
	const bool enableVulkanValidationLayers = false;
#else         
	// Debug Mode:
	const bool enableVulkanValidationLayers = true;
#endif        


	// Member Methods:
	void initWindow();
	void initVulkan();	
	void mainLoop();
	void cleanup();

	// Helper Methods:
	void createVulkanInstance();
	void createVulkanSurface();
	void pickVulkanPhysicalDevice();
	void createLogicalDevice();
	void recreateSwapChain();
	void cleanupSwapChain();
	void createSwapChain();
	void createSwapChainImageViews();
	void createRenderPass();
	void createGraphicsPipeline();
	void createFramebuffers();
	bool isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableSurfaceFormats);
	VkPresentModeKHR chooseSwapPresentationMode(const std::vector<VkPresentModeKHR>& availablePresentationModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
	bool checkValidationLayersSupport();
	bool checkPhysicalDeviceExtensionsSupport(VkPhysicalDevice physicalDevice);
	VkShaderModule createShaderModule(const std::vector<char>& compiledShaderCode);
	void createCommandPool();
	void createCommandBuffers();
	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t swapChainImageIndex);
	void createSynchronizationObjects();
	void drawFrame();

	// static methods:
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
	static std::vector<char> readFile(const std::string& fileName);

};


// Struct definitions:

/// @brief Custom struct that holds the queue indices for various device queue families.
struct QueueFamilyIndices {
	/// @brief The index of the Graphics queue family (if any) of the GPU.
	std::optional<uint32_t> graphicsFamily;
	/// @brief The index of the Presentation queue family (if any) of the GPU.
	std::optional<uint32_t> presentationFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentationFamily.has_value();
	}
};

/// @brief Custom struct that holds the data relevant to the Swapchain support of the Physical Device (GPU).
struct SwapChainSupportDetails {
	/// @brief The capabilities of the surface supported by our GPU (eg: min/max images in Swapchain).
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	/// @brief List of pixel formats and color spaces supported by our GPU (eg: SRGB color space).
	std::vector<VkSurfaceFormatKHR> surfaceFormats;
	/// @brief List of presentation modes supported for the Swapchain (eg: FIFO, Mailbox etc.).
	std::vector<VkPresentModeKHR> presentationModes;
};

