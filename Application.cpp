
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
	createVulkanSurface();
	pickVulkanPhysicalDevice();
	createLogicalDevice();
	createSwapChain();
}

void Application::mainLoop() {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
}

void Application::cleanup() {
	// Destroy Vulkan instance just before the program terminates
	vkDestroyDevice(vulkanLogicalDevice, nullptr);
	vkDestroySurfaceKHR(vulkanInstance, vulkanSurface, nullptr);
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

void Application::createVulkanSurface() {
	VkResult result = glfwCreateWindowSurface(vulkanInstance, window, nullptr, &vulkanSurface);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("RUNTIME ERROR: Failed to create Vulkan surface!");
	}
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
	std::cout << "> Vulkan picked the physical device (GPU): '" << physicalDeviceProperties.deviceName << "'" << std::endl;
#endif

}

void Application::createLogicalDevice() {
	// Logical Device is created based on the Physical Device selected
	if (vulkanPhysicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("RUNTIME ERROR: Unable to create Vulkan logical device! Physical device is NULL or hasn't been created yet...");
	}
	// Get the actual indices of the queue families that we'll need (in this case Graphics queue)
	QueueFamilyIndices queueFamilyIndices = findQueueFamilies(vulkanPhysicalDevice);

	// Specify to Vulkan which Queues must be created and how many of them, along with the priority of each queue
	std::set<uint32_t> requiredQueueFamilies = {queueFamilyIndices.graphicsFamily.value(), queueFamilyIndices.presentationFamily.value()};
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos {};

	float queuePriority{ 1.0f };
	for (uint32_t queueFamily : requiredQueueFamilies) {
		// Create a Vulkan queue create info struct for each queue family
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		// Append the struct to the vector of queue create info structs (pushes a copy)
		queueCreateInfos.push_back(queueCreateInfo);
	}

	// Specifying the physical device features we'll be using (eg. geometry shader)
	VkPhysicalDeviceFeatures physicalDeviceFeatures{};

	// Specify how to create the Logical Device to Vulkan
	VkDeviceCreateInfo createDeviceInfo{};
	createDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createDeviceInfo.pQueueCreateInfos = queueCreateInfos.data();
	createDeviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createDeviceInfo.pEnabledFeatures = &physicalDeviceFeatures;
	createDeviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
	createDeviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createDeviceInfo.enabledLayerCount = 0;
	if (enableVulkanValidationLayers) {
		createDeviceInfo.enabledLayerCount = static_cast<uint32_t>(vulkanValidationLayers.size());
		createDeviceInfo.ppEnabledLayerNames = vulkanValidationLayers.data();
	}

	// Create the Logical Device
	VkResult result = vkCreateDevice(vulkanPhysicalDevice, &createDeviceInfo, nullptr, &vulkanLogicalDevice);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("RUNTIME ERROR: Failed to create Vulkan logical device!");
	}

	// Logical Device is successfully created...
	std::cout << "> Vulkan logical device successfully created." << std::endl;

	// Get the queue handles:
	vkGetDeviceQueue(vulkanLogicalDevice, queueFamilyIndices.graphicsFamily.value(), 0, &deviceGraphicsQueue);
	vkGetDeviceQueue(vulkanLogicalDevice, queueFamilyIndices.presentationFamily.value(), 0, &devicePresentationQueue);
	std::cout << "> Retrieved queue handles." << std::endl;

}

void Application::createSwapChain() {
	// Get supported swapchain properties from the physical device (GPU)
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(vulkanPhysicalDevice);

	// Choose and set the desired properties of our swapchain
	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.surfaceFormats);
	VkPresentModeKHR presentationMode = chooseSwapPresentationMode(swapChainSupport.presentationModes);
	VkExtent2D swapExtent = chooseSwapExtent(swapChainSupport.surfaceCapabilities);

	// We would like one image more than the min supported images in the swapchain by the device (ensured that its clamped)
	uint32_t swapChainImagesCount = std::clamp(swapChainSupport.surfaceCapabilities.minImageCount + 1, swapChainSupport.surfaceCapabilities.minImageCount, swapChainSupport.surfaceCapabilities.maxImageCount);

	// Create the SwapChain:
	VkSwapchainCreateInfoKHR swapChainCreateInfo{};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = vulkanSurface;
	swapChainCreateInfo.minImageCount = swapChainImagesCount;
	swapChainCreateInfo.imageFormat = surfaceFormat.format;
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapChainCreateInfo.imageExtent = swapExtent;
	swapChainCreateInfo.imageArrayLayers = 1;  // Layers in each image (will always be 1, unless building a stereoscopic 3D application)
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

}

bool Application::isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice) {
	// We're deeming a GPU as suitable if it has the Queue Families that we need (eg. Graphics family)
	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
	bool deviceExtensionsSupported = checkPhysicalDeviceExtensionsSupport(physicalDevice);
	bool swapChainSupportAdequate{ false };
	if (deviceExtensionsSupported) {
		SwapChainSupportDetails swapChainSupportDetails = querySwapChainSupport(physicalDevice);
		// If we have atleast 1 surface format and 1 presentation mode supported, thats adequate for now
		swapChainSupportAdequate = !swapChainSupportDetails.surfaceFormats.empty() && !swapChainSupportDetails.presentationModes.empty();
	}
	// IMPORTANT: Need to check for 'swapChainSupportAdequate' AFTER 'deviceExtensionsSupported' due to the way AND conditions work
	return indices.isComplete() && deviceExtensionsSupported && swapChainSupportAdequate;
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
		// Check for presentation support by the queue family
		VkBool32 presentationSupport{ false };
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, vulkanSurface, &presentationSupport);

		// Check if queue family supports graphics queue
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}
		// If current queue family supports presentation, set its family index
		if (presentationSupport) {
			indices.presentationFamily = i;
		}	
		if (indices.isComplete()) {
			// All required queues were found
			break;
		}
		++i;
	}

	return indices;
}

SwapChainSupportDetails Application::querySwapChainSupport(VkPhysicalDevice physicalDevice) {
	SwapChainSupportDetails swapchainSupportDetails{};

	// Get the surface capabilities of the physical device
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, vulkanSurface, &swapchainSupportDetails.surfaceCapabilities);

	// Get the supported surface formats by the physical device
	uint32_t surfaceFormatsCount{};
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, vulkanSurface, &surfaceFormatsCount, nullptr);
	if (surfaceFormatsCount != 0) {
		swapchainSupportDetails.surfaceFormats.resize(surfaceFormatsCount);
	}
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, vulkanSurface, &surfaceFormatsCount, swapchainSupportDetails.surfaceFormats.data());

	// Get the supported presentation modes by the physical device
	uint32_t presentationModesCount{};
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, vulkanSurface, &presentationModesCount, nullptr);
	if (presentationModesCount != 0) {
		swapchainSupportDetails.presentationModes.resize(presentationModesCount);
	}
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, vulkanSurface, &presentationModesCount, swapchainSupportDetails.presentationModes.data());

	return swapchainSupportDetails;
}

VkSurfaceFormatKHR Application::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableSurfaceFormats) {
	for (const VkSurfaceFormatKHR& availableSurfaceFormat : availableSurfaceFormats) {
		// We'd prefer to have the BGR (8-bit) format and the SRGB non linear color space for our surface
		if (availableSurfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableSurfaceFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
			// If supported, return it
			return availableSurfaceFormat;
		}
	}
	// Desired format not supported (return the first supported one)
	// Can go deeper and perform ranking of the available formats and colorspaces to pick the best too...
	return availableSurfaceFormats.at(0);
}

VkPresentModeKHR Application::chooseSwapPresentationMode(const std::vector<VkPresentModeKHR>& availablePresentationModes) {
	// We'll prefer the MAILBOX presentation mode (suitable for desktop apps, but not for mobile apps due to power consumption & wastage of images)
	for (const VkPresentModeKHR& presentationMode : availablePresentationModes) {
		if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return presentationMode;
		}
	}
	// FIFO is guaranteed to exist, if we couldn't find the best one
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Application::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) {
	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		// If its not max, that means the GPU requires a fixed width & height for the swapchain (eg: mobile GPUs)
		return surfaceCapabilities.currentExtent;
	}
	int width{};
	int height{};
	glfwGetFramebufferSize(window, &width, &height);

	VkExtent2D actualExtent = {
		static_cast<uint32_t>(width),
		static_cast<uint32_t>(height)
	};
	actualExtent.width = std::clamp(actualExtent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
	actualExtent.height = std::clamp(actualExtent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);

	return actualExtent;
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

bool Application::checkPhysicalDeviceExtensionsSupport(VkPhysicalDevice physicalDevice) {
	// Get the available device extensions
	uint32_t availableExtensionsCount{};
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableExtensionsCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(availableExtensionsCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableExtensionsCount, availableExtensions.data());

	// Create a set of required extensions (copies unique contents from deviceExtensions member and stores)
	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	// Tick off all the required extensions that are already there
	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	// Return true if all the required extensions are already existing (set is empty; everything ticked off)
	return requiredExtensions.empty();
}

