#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>

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


	// Methods:
	void initWindow();
	void initVulkan();
	void createVulkanInstance();
	void mainLoop();
	void cleanup();
};

