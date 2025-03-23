
#include "Application.h"

void Application::run() {
	initWindow();
	initVulkan();
	mainLoop();
	cleanup();
}

void Application::initWindow() {
	glfwInit();
	// Defaults to OpenGL hence specifying no API
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	// Resizing windows in Vulkan requires extra steps
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(WIDTH, HEIGHT, APPLICATION_NAME, nullptr, nullptr);
}

void Application::initVulkan() {
	createVulkanInstance();
	pickVulkanPhysicalDevice();
}

void Application::mainLoop() {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
}

void Application::cleanup() {
	// Destroy Vulkan instance just before the program terminates
	vkDestroyInstance(vulkanInstance, nullptr);
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Application::createVulkanInstance() {
	// Check if validation layers were enabled. If so, check if they're all supported
	if (enableVulkanValidationLayers == true) {
		std::cout << "> Vulkan validation layers requested." << std::endl;
		if (!checkValidationLayersSupport())
			throw std::runtime_error("RUNTIME ERROR: Not all validation layers requested are available!");
		else
			std::cout << "> All requested validation layers are supported." << std::endl;
	}

	// [Optional struct] Provides metadata of the application
	VkApplicationInfo vulkanAppInfo{};
	vulkanAppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	vulkanAppInfo.pApplicationName = APPLICATION_NAME;
	vulkanAppInfo.apiVersion = VK_MAKE_VERSION(1, 4, 3);
	vulkanAppInfo.pEngineName = "No Engine";
	vulkanAppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	vulkanAppInfo.apiVersion = VK_API_VERSION_1_4;

	// Vulkan needs extensions to deal with GLFW (GLFW provides handy methods to get these extension names)
	uint32_t glfwExtensionsCount{};
	const char** glfwExtensionNames;
	glfwExtensionNames = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);

#ifdef NDEBUG
	// Release Mode:
#else
	// Debug Mode:
	// List down all the available Vulkan instance extensions
	uint32_t vulkanExtensionsCount{};
	vkEnumerateInstanceExtensionProperties(nullptr, &vulkanExtensionsCount, nullptr);
	std::vector<VkExtensionProperties> vulkanExtensions(vulkanExtensionsCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &vulkanExtensionsCount, vulkanExtensions.data());
	std::cout << "DEBUG LOG: Available Vulkan Extensions:" << std::endl;
	for (const VkExtensionProperties& extensionProperty : vulkanExtensions) {
		std::cout << "\t" << extensionProperty.extensionName << " (version: " << extensionProperty.specVersion << ")" << std::endl;
	}
	std::cout << std::endl;

	// List down all the GLFW extensions required for Vulkan
	std::cout << "DEBUG LOG: Required GLFW Extensions for Vulkan:" << std::endl;
	bool glfwExtensionFound{ false };
	for (int i{ 0 }; i < glfwExtensionsCount; i++) {
		std::cout << "\t" << glfwExtensionNames[i];
		// Print if the GLFW extensions are available in the Vulkan instance extensions
		for (const VkExtensionProperties& extensionProperty : vulkanExtensions) {
			if (strcmp(extensionProperty.extensionName, glfwExtensionNames[i]) == 0) {
				std::cout << " - (SUPPORTED BY VULKAN INSTANCE)" << std::endl;
				glfwExtensionFound = true;
				break;
			}
		}
		if (!glfwExtensionFound) {
			std::cout << "\t(!UNSUPPORTED!)" << std::endl;
			glfwExtensionFound = false;
			throw std::runtime_error("RUNTIME ERROR: Unsupported GLFW extensions found!");
		}
	}
#endif

	// [Required struct] Tells Vulkan how to create the instance
	VkInstanceCreateInfo vulkanCreateInfo{};
	vulkanCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	vulkanCreateInfo.pApplicationInfo = &vulkanAppInfo;
	vulkanCreateInfo.enabledExtensionCount = glfwExtensionsCount;
	vulkanCreateInfo.ppEnabledExtensionNames = glfwExtensionNames;
	vulkanCreateInfo.enabledLayerCount = 0;                          // Disabling validation layers for now (they help in debugging Vulkan)

	// Creates the instance based on the Create Info struct, and stores the instance in the specified instance variable (3rd arg)
	VkResult result = vkCreateInstance(&vulkanCreateInfo, nullptr, &vulkanInstance);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("RUNTIME ERROR: Failed to create Vulkan instance!");
	}
	std::cout << "> Vulkan instance created successfully." << std::endl;

}

void Application::pickVulkanPhysicalDevice() {
	// Get the list of physical devices (GPUs) available in the system that support Vulkan
	uint32_t physicalDevicesCount{};
	vkEnumeratePhysicalDevices(vulkanInstance, &physicalDevicesCount, nullptr);
	if (physicalDevicesCount == 0) {
		throw std::runtime_error("RUNTIME ERROR: Failed to find physical devices that support Vulkan!");
	}
	std::vector<VkPhysicalDevice> physicalDevicesList(physicalDevicesCount);
	vkEnumeratePhysicalDevices(vulkanInstance, &physicalDevicesCount, physicalDevicesList.data());

	VkPhysicalDeviceProperties physicalDeviceProperties;
	// Find the most suitable GPU
	for (const VkPhysicalDevice& physicalDevice : physicalDevicesList) {
		// Prioritize picking the Discrete GPU (if it exists)
		vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

		if (isPhysicalDeviceSuitable(physicalDevice)) {
			vulkanPhysicalDevice = physicalDevice;
			// Suitable discrete GPU found. Choose that as our Vulkan physical device.
			if (physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {				
				break;
			}
		} 
	}
	if (vulkanPhysicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("RUNTIME ERROR: No suitable physical device found!");
	}

#ifdef NDEBUG
	// Release Mode:
#else
	// Debug Mode:
	vkGetPhysicalDeviceProperties(vulkanPhysicalDevice, &physicalDeviceProperties);
	std::cout << "> Picked the physical device (GPU): '" << physicalDeviceProperties.deviceName << "'" << std::endl;
#endif

}

bool Application::isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice) {
	// We're deeming a GPU as suitable if it has the Queue Families that we need (eg. Graphics family)
	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
	return indices.isComplete();
}

QueueFamilyIndices Application::findQueueFamilies(VkPhysicalDevice physicalDevice) {
	QueueFamilyIndices indices;

	// Get the list of queue families
	uint32_t queueFamilyCount{};
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

	// Need to find at least one queue family that supports VK_QUEUE_GRAPHICS_BIT
	size_t i{ 0 };
	for (const auto& queueFamily: queueFamilies) {
		if (indices.isComplete())
			break;
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
			break;
		}
		++i;
	}

	return indices;
}

/// @brief Checks if all the validation layers requested are supported by Vulkan or not.
/// @return boolean True if all the validation layers are supported. False otherwise.
bool Application::checkValidationLayersSupport() {
	// Get all the available Vulkan instance layers
	uint32_t layerCount{};
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> availabeInstanceLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availabeInstanceLayers.data());

	// Check if all the requested layers are supported (found) or not
	for (const char* requestedLayer : vulkanValidationLayers) {
		bool layerFound{ false };
		for (const auto& availableLayerProperty : availabeInstanceLayers) {
			if (strcmp(requestedLayer, availableLayerProperty.layerName) == 0) {
				layerFound = true;
				break;
			}
		}
		if (!layerFound) {
			// Requested validation layers were NOT found
			return false;
		}
	}

	// All requested validation layers were found
	return true;
}

