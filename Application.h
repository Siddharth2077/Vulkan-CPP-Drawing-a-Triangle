#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <optional>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <set>

// Forward declarations
struct QueueFamilyIndices;

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
	VkPhysicalDevice vulkanPhysicalDevice = VK_NULL_HANDLE;
	VkDevice vulkanLogicalDevice = VK_NULL_HANDLE;
	VkQueue deviceGraphicsQueue = VK_NULL_HANDLE;
	VkQueue devicePresentationQueue = VK_NULL_HANDLE;
	VkSurfaceKHR vulkanSurface = VK_NULL_HANDLE;
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
	bool isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice);
	bool checkValidationLayersSupport();
	bool checkPhysicalDeviceExtensionsSupport(VkPhysicalDevice physicalDevice);

};


// Struct definitions:

/// @brief Custom struct that holds the queue indices for various device queue families.
struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentationFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentationFamily.has_value();
	}
};

