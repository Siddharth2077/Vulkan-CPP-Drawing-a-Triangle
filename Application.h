#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <vector>

class Application {
public:
	void run();

private:
	// Members:
	GLFWwindow* window;
	const uint32_t WIDTH{ 800 };
	const uint32_t HEIGHT{ 600 };
	const char* APPLICATION_NAME = "Vulkan Application";

	VkInstance vulkanInstance;
	const std::vector<const char*> vulkanValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

#ifdef NDEBUG 
	// Release Mode:
	const bool enableVulkanValidationLayers = false;
#else         
	// Debug Mode:
	const bool enableVulkanValidationLayers = true;
#endif        


	// Methods:
	void initWindow();
	void initVulkan();
	void createVulkanInstance();
	void mainLoop();
	void cleanup();

	// Helper Methods:
	bool checkValidationLayersSupport();

};

