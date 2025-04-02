
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
	createSwapChainImageViews();
	createGraphicsPipeline();
}

void Application::mainLoop() {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
}

void Application::cleanup() {
	// Destroy the swapchain image-views
	for (VkImageView imageView : vulkanSwapChainImageViews) {
		vkDestroyImageView(vulkanLogicalDevice, imageView, nullptr);
	}
	vkDestroySwapchainKHR(vulkanLogicalDevice, vulkanSwapChain, nullptr);
	vkDestroyDevice(vulkanLogicalDevice, nullptr);
	vkDestroySurfaceKHR(vulkanInstance, vulkanSurface, nullptr);
	// Destroy Vulkan instance just before the program terminates
	vkDestroyInstance(vulkanInstance, nullptr);
	glfwDestroyWindow(window);
	glfwTerminate();
}


void Application::createVulkanInstance() {
	// Check if validation layers were enabled. If so, check if they're all supported
	if (enableVulkanValidationLayers == true) {
		std::cout << "> Vulkan validation layers requested.\n";
		if (!checkValidationLayersSupport())
			throw std::runtime_error("RUNTIME ERROR: Not all validation layers requested are available!");
		else
			std::cout << "> All requested validation layers are supported.\n";
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
	std::cout << "DEBUG LOG: Available Vulkan Extensions:\n";
	for (const VkExtensionProperties& extensionProperty : vulkanExtensions) {
		std::cout << "\t" << extensionProperty.extensionName << " (version: " << extensionProperty.specVersion << ")\n";
	}

	// List down all the GLFW extensions required for Vulkan
	std::cout << "DEBUG LOG: Required GLFW Extensions for Vulkan:\n";
	bool glfwExtensionFound{ false };
	for (int i{ 0 }; i < glfwExtensionsCount; i++) {
		std::cout << "\t" << glfwExtensionNames[i];
		// Print if the GLFW extensions are available in the Vulkan instance extensions
		for (const VkExtensionProperties& extensionProperty : vulkanExtensions) {
			if (strcmp(extensionProperty.extensionName, glfwExtensionNames[i]) == 0) {
				std::cout << " - (SUPPORTED BY VULKAN INSTANCE)\n";
				glfwExtensionFound = true;
				break;
			}
		}
		if (!glfwExtensionFound) {
			std::cout << "\t(!UNSUPPORTED!)\n";
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
	std::cout << "> Vulkan instance created successfully.\n";

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
	std::cout << "> Vulkan picked the physical device (GPU): '" << physicalDeviceProperties.deviceName << "'\n";
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
	std::cout << "> Vulkan logical device successfully created.\n";

	// Get the queue handles:
	vkGetDeviceQueue(vulkanLogicalDevice, queueFamilyIndices.graphicsFamily.value(), 0, &deviceGraphicsQueue);
	vkGetDeviceQueue(vulkanLogicalDevice, queueFamilyIndices.presentationFamily.value(), 0, &devicePresentationQueue);
	std::cout << "> Retrieved queue handles.\n";

}

void Application::createSwapChain() {
	// Safety Check (although will never reach here)
	if (vulkanPhysicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("RUNTIME ERROR: Failed to create SwapChain! Physical Device is NULL.");
	}
	if (vulkanLogicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("RUNTIME ERROR: Failed to create SwapChain! Logical Device is NULL.");
	}

	// Get supported swapchain properties from the physical device (GPU)
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(vulkanPhysicalDevice);

	// Choose and set the desired properties of our swapchain
	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.surfaceFormats);
	VkPresentModeKHR presentationMode = chooseSwapPresentationMode(swapChainSupport.presentationModes);
	VkExtent2D swapExtent = chooseSwapExtent(swapChainSupport.surfaceCapabilities);

	// We would like one image more than the min supported images in the swapchain by the device (ensured that its clamped)
	uint32_t swapChainImagesCount = std::clamp(swapChainSupport.surfaceCapabilities.minImageCount + 1, swapChainSupport.surfaceCapabilities.minImageCount, swapChainSupport.surfaceCapabilities.maxImageCount);

	// Specify how to create the SwapChain:
	VkSwapchainCreateInfoKHR swapChainCreateInfo{};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = vulkanSurface;
	swapChainCreateInfo.minImageCount = swapChainImagesCount;
	swapChainCreateInfo.imageFormat = surfaceFormat.format;
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapChainCreateInfo.imageExtent = swapExtent;
	swapChainCreateInfo.imageArrayLayers = 1;  // Layers in each image (will always be 1, unless building a stereoscopic 3D application)
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	// Need to specify how to handle swapchain images that will be used across multiple queue families (eg: graphics & presentation queues)
	QueueFamilyIndices queueFamilyIndices = findQueueFamilies(vulkanPhysicalDevice);
	uint32_t indices[] = { queueFamilyIndices.graphicsFamily.value(), queueFamilyIndices.presentationFamily.value()};
	if (queueFamilyIndices.graphicsFamily.value() != queueFamilyIndices.presentationFamily.value()) {
		// The graphics and presentation families are different
		// We'll go with CONCURRENT mode (less efficient but Vulkan automatically handles resource sharing and ownership transfer)
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainCreateInfo.pQueueFamilyIndices = indices;
		swapChainCreateInfo.queueFamilyIndexCount = 2;

	} else {
		// The graphics and presentation families are the same
		// Go with EXCLUSIVE mode (most efficient and best performance)
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapChainCreateInfo.pQueueFamilyIndices = nullptr;  // optional
		swapChainCreateInfo.queueFamilyIndexCount = 0;      // optional
	}
	// Not applying any pre-transform to swapchain images (used in mobile applications)
	swapChainCreateInfo.preTransform = swapChainSupport.surfaceCapabilities.currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;  // No blending with other windows in the window system
	swapChainCreateInfo.presentMode = presentationMode;
	swapChainCreateInfo.clipped = VK_TRUE;  // Don't care about pixels that are obscured by say, other windows for example
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;  // Swapchain might be unoptimized over time, recreating required. Specify the old one here.

	// Create the SwapChain:
	VkResult result = vkCreateSwapchainKHR(vulkanLogicalDevice, &swapChainCreateInfo, nullptr, &vulkanSwapChain);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("RUNTIME ERROR: Failed to create the SwapChain!");
	}
	std::cout << "> Vulkan swapchain created successfully.\n";

	// Store the swapchain image-format and extent in member variables:
	vulkanSwapChainImageFormat = surfaceFormat.format;
	vulkanSwapChainImageColorspace = surfaceFormat.colorSpace;
	vulkanSwapChainExtent = swapExtent;

	// Retrieve the handles to the swapchain images
	vkGetSwapchainImagesKHR(vulkanLogicalDevice, vulkanSwapChain, &swapChainImagesCount, nullptr);
	vulkanSwapChainImages.resize(swapChainImagesCount);
	vkGetSwapchainImagesKHR(vulkanLogicalDevice, vulkanSwapChain, &swapChainImagesCount, vulkanSwapChainImages.data());
	std::cout << "> Retrieved swapchain image handles.\n";

}

void Application::createSwapChainImageViews() {
	// FResize the vector holding the image-views to fit the number of images
	vulkanSwapChainImageViews.resize(vulkanSwapChainImages.size());
	// Iterate through the swapchain images (not views)
	for (size_t i{ 0 }; i < vulkanSwapChainImages.size(); i++) {
		// Create the image-view for each swapchain image
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = vulkanSwapChainImages.at(i);
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = vulkanSwapChainImageFormat;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

		VkResult result = vkCreateImageView(vulkanLogicalDevice, &imageViewCreateInfo, nullptr, &vulkanSwapChainImageViews.at(i));
		if (result != VK_SUCCESS) {
			throw std::runtime_error("RUNTIME ERROR: Failed to create image-views for swapchain images!");
		}
	}
	std::cout << "> Created image-views for swapchain images successfully.\n";
}

void Application::createGraphicsPipeline() {
	// Read in the compiled Vertex and Fragment shaders (Spir-V)
	auto vertShaderCode = readFile("shaders/vert.spv");
	auto fragShaderCode = readFile("shaders/frag.spv");

	// Create the shader modules from the compiled shader code
	VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
	VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";  // the entry point of the shader code

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";  // the entry point of the shader code

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

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

/// @brief Creates and returns a VkShaderModule wrapper around the Spir-V compiled shader code.
VkShaderModule Application::createShaderModule(const std::vector<char>& compiledShaderCode) {
	VkShaderModuleCreateInfo shaderModuleCreateInfo{};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = compiledShaderCode.size();
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(compiledShaderCode.data());
	
	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(vulkanLogicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("RUNTIME ERROR: Failed to create shader module!");
	}
	return shaderModule;
}


/// @brief Reads all the bytes from a specified file and return them in a byte array (vector).
std::vector<char> Application::readFile(const std::string& fileName) {
	// Benefit of starting at end of file is that we can immediately get the size and allocate a buffer accordingly
	std::ifstream file(fileName, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("RUNTIME ERROR: Failed to open file '" + fileName + "'.");
	}
	// File is now open..
	// 
	// Get the file size and create a buffer of that size
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	// Read the contents of the whole file at once, into the buffer
	file.seekg(0);
	file.read(buffer.data(), fileSize);

	// Close the file
	file.close();
	return buffer;
}

