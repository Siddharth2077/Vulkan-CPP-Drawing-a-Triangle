
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
	createRenderPass();
	createGraphicsPipeline();
	createFramebuffers();
	createCommandPool();
	createCommandBuffer();
	createSynchronizationObjects();
}

void Application::mainLoop() {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		drawFrame();
	}
	// Wait for the logical device to finish operations before destroying the window
	vkDeviceWaitIdle(vulkanLogicalDevice);
}

void Application::cleanup() {
	// Destroy synchronization objects
	vkDestroySemaphore(vulkanLogicalDevice, imageAvailableSemaphore, nullptr);
	vkDestroySemaphore(vulkanLogicalDevice, renderFinishedSemaphore, nullptr);
	vkDestroyFence(vulkanLogicalDevice, inFlightFence, nullptr);
	// Destroy command buffer pool
	vkDestroyCommandPool(vulkanLogicalDevice, vulkanCommandPool, nullptr);
	// Delete all the framebuffers
	for (auto framebuffer : vulkanSwapChainFramebuffers) {
		vkDestroyFramebuffer(vulkanLogicalDevice, framebuffer, nullptr);
	}
	vkDestroyPipeline(vulkanLogicalDevice, vulkanGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(vulkanLogicalDevice, vulkanPipelineLayout, nullptr);
	vkDestroyRenderPass(vulkanLogicalDevice, vulkanRenderPass, nullptr);
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

void Application::createRenderPass() {
	// We'll just have one color buffer attachment for our framebuffer represented by one of the swapchain images
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = vulkanSwapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;  // No MSAA (hence 1 sample per pixel)
	// Color buffer load and store specification:
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	// We're not using the stencil buffer right now:
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	// Initial and Final states of the Images before and after the render pass
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // Images to be used for presentation in swap chain


	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Subpasses (We'll have only one subpass for now)
	// There can be multiple subpasses when there's postprocessing needed, for example.
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;  // Stating that this is a graphics subpass
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.colorAttachmentCount = 1;

	// Subpass Dependency management:
	VkSubpassDependency subpassDependency{};
	subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependency.dstSubpass = 0;
	subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependency.srcAccessMask = 0;
	subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;


	// Render Pass creation
	VkRenderPassCreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.pAttachments = &colorAttachment;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pDependencies = &subpassDependency;
	renderPassCreateInfo.dependencyCount = 1;

	VkResult result = vkCreateRenderPass(vulkanLogicalDevice, &renderPassCreateInfo, nullptr, &vulkanRenderPass);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("RUNTIME ERROR: Failed to create render pass!");
	}
	std::cout << "> Created render pass successfully.\n";

}

void Application::createGraphicsPipeline() {
	// Read in the compiled Vertex and Fragment shaders (Spir-V)
	auto vertShaderCode = readFile("shaders/vert.spv");
	auto fragShaderCode = readFile("shaders/frag.spv");

	// Create the shader modules from the compiled shader code and assign them to their respective pipeline stages
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

	// Describing the vertex input to the Vulkan vertex shader
	VkPipelineVertexInputStateCreateInfo vertexDataInputInfo{};
	vertexDataInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexDataInputInfo.pVertexBindingDescriptions = nullptr;
	vertexDataInputInfo.vertexBindingDescriptionCount = 0;
	vertexDataInputInfo.pVertexBindingDescriptions = nullptr;
	vertexDataInputInfo.vertexAttributeDescriptionCount = 0;
		
	// Input assembler creation description:
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	// Viewport definition
	VkViewport viewport{};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = (float)vulkanSwapChainExtent.width;
	viewport.height = (float)vulkanSwapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// Scissor definition (we'll just draw the entire framebuffer for now)
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = vulkanSwapChainExtent;
		
	// Make certain parts of the pipeline dynamic [Viewport and Scissor]
	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamicPipelineCreateInfo{};
	dynamicPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicPipelineCreateInfo.pDynamicStates = dynamicStates.data();
	dynamicPipelineCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());

	// Telling Vulkan about our Viewports and Scissors (only mention the counts for a dynamic version of the two)
	// If we need a static viewport and scissor (unlikely), just pass the reference to the viewport and scissor now
	// Since we're using a dynamic viewport and scissor, we'll be passing them later using 'vkCmd' commands
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.scissorCount = 1;

	// Rasterizer info
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;  // Disable any output to the framebuffer (geometry never passes through the rasterizer)
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	// Vulkan uses the order of the vertices when projected on to the screen to determine front/back faces
	// We're telling Vulkan what order (clockwise/anti-clockwise) must be treated as front face [OBJ, GLTF etc. use anti-clockwise]
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	// Multisampling (Anti-Aliasing) - Disabled for now
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	// Color Blend attachment properties (per attached framebuffer)
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = 
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	// Opaque rendering (no color blending) - What this means is new colors simply overwrite the old colors of the frame buffer
	colorBlendAttachment.blendEnable = VK_FALSE;

	// Color Blending stage properties (global color blend settings)
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.attachmentCount = 1;

	// Defining the Pipeline layout (specifies the 'uniforms' (global shader variables) that can be changed at runtime)
	// Creating an empty pipeline layout for now
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	// Create the pipeline layout
	VkResult result = vkCreatePipelineLayout(vulkanLogicalDevice, &pipelineLayoutCreateInfo, nullptr, &vulkanPipelineLayout);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("RUNTIME ERROR: Failed to create pipeline layout!");
	}
	std::cout << "> Created pipeline layout successfully.\n";


	// Specify how to create the Graphics Pipeline:
	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pStages = shaderStages;  // The vertex and fragment shader stages (array created above)
	graphicsPipelineCreateInfo.stageCount = 2;
	// Specifying each stage of the pipeline:
	graphicsPipelineCreateInfo.pVertexInputState = &vertexDataInputInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizer;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampling;
	graphicsPipelineCreateInfo.pDepthStencilState = nullptr;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlending;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicPipelineCreateInfo;
	// Pipeline layout:
	graphicsPipelineCreateInfo.layout = vulkanPipelineLayout;
	// Render pass and Sub passes:
	graphicsPipelineCreateInfo.renderPass = vulkanRenderPass;
	graphicsPipelineCreateInfo.subpass = 0;
	// Possible to derive a pipeline more efficienbtly from an existing pipeline (if they share a lot in common)
	// We won't be using this feature here, since we have only one graphics pipeline
	graphicsPipelineCreateInfo.basePipelineHandle = nullptr;
	graphicsPipelineCreateInfo.basePipelineIndex = -1;

	// Create the Graphics Pipeline:
	result = vkCreateGraphicsPipelines(
		vulkanLogicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &vulkanGraphicsPipeline
	);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("RUNTIME ERROR: Failed to create Vulkan Graphics Pipeline!");
	}
	std::cout << "> Vulkan graphics pipeline created successfully.\n";

	vkDestroyShaderModule(vulkanLogicalDevice, vertShaderModule, nullptr);
	vkDestroyShaderModule(vulkanLogicalDevice, fragShaderModule, nullptr);
}

void Application::createFramebuffers() {
	// Each framebuffer wraps a SwapChain image view
	vulkanSwapChainFramebuffers.resize(vulkanSwapChainImageViews.size());

	for (size_t i{ 0 }; i < vulkanSwapChainImageViews.size(); i++) {
		VkImageView framebufferAttachments[] = {
			// We're only attaching the Color attachment for now 
			// Can have Depth, Stencil and Resolve[MSAA] in the future per framebuffer
			vulkanSwapChainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferCreateInfo{};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = vulkanRenderPass;
		framebufferCreateInfo.pAttachments = framebufferAttachments;
		framebufferCreateInfo.attachmentCount = 1;  // Only Color attachment for now
		framebufferCreateInfo.width = vulkanSwapChainExtent.width;
		framebufferCreateInfo.height = vulkanSwapChainExtent.height;
		framebufferCreateInfo.layers = 1;  // 2D Images (Will be > 1 for Stereoscopic 3D)

		VkResult result = vkCreateFramebuffer(vulkanLogicalDevice, &framebufferCreateInfo, nullptr, &vulkanSwapChainFramebuffers.at(i));
		if (result != VK_SUCCESS) {
			throw std::runtime_error("RUNTIME ERROR: Failed to create Framebuffers!");
		}
		std::cout << "> Created Vulkan swapchain framebuffer for view " << i << " successfully.\n";

	}
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

void Application::createCommandPool() {
	// Fetch the queue families of the GPU
	QueueFamilyIndices queueFamilies = findQueueFamilies(vulkanPhysicalDevice);

	// Specify how to create the Command Pool
	VkCommandPoolCreateInfo commandPoolCreateInfo{};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	// This flag provides more fine grain control over individual command buffer in this pool
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	// Each command pool can only allocate command buffers that are submitted on a single type of queue (In this case Graphics queue for draw cmds)
	commandPoolCreateInfo.queueFamilyIndex = queueFamilies.graphicsFamily.value();


	// Create the Command Pool
	VkResult result = vkCreateCommandPool(vulkanLogicalDevice, &commandPoolCreateInfo, nullptr, &vulkanCommandPool);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("RUNTIME ERROR: Failed to create Command Pool.");
	}
	std::cout << "> Created Vulkan command pool successfully.\n";
}

void Application::createCommandBuffer() {
	// Specify how to allocate command buffers and from which command pool
	/*
		Note:
		- VK_COMMAND_BUFFER_LEVEL_PRIMARY:		Can be submitted to a queue for execution, but cannot be called from other command buffers.
		- VK_COMMAND_BUFFER_LEVEL_SECONDARY:	Cannot be submitted directly, but can be called from primary command buffers
	*/
	VkCommandBufferAllocateInfo commandBuffersAllocateInfo{};
	commandBuffersAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBuffersAllocateInfo.commandPool = vulkanCommandPool;
	commandBuffersAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBuffersAllocateInfo.commandBufferCount = 1;  // One Graphics command buffer for draw cmds

	// Allocate the Command Buffer(s):
	VkResult result = vkAllocateCommandBuffers(vulkanLogicalDevice, &commandBuffersAllocateInfo, &vulkanCommandBuffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("RUNTIME ERROR: Failed to allocate Command Buffers from Pool!\n");
	}
	std::cout << "> Created Vulkan command buffers successfully.\n";

}

/// @brief Function that writes the commands we want to execute into a command buffer.
/// @param commandBuffer: The command buffer (VkCommandBuffer object) that you want to write the command to.
/// @param swapChainImageIndex: The index of the SwapChain image that you want to write to.
void Application::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t swapChainImageIndex) {
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;  // optional
	beginInfo.pInheritanceInfo = nullptr;  // optional (only relevant for Secondary Command Buffers)

	VkResult result = vkBeginCommandBuffer(vulkanCommandBuffer, &beginInfo);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("RUNTIME ERROR: Failed to begin recording Command Buffer!");
	}

	// Begin the Render Pass
	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = vulkanRenderPass;
	renderPassBeginInfo.framebuffer = vulkanSwapChainFramebuffers.at(swapChainImageIndex);
	renderPassBeginInfo.renderArea.offset = { 0,0 };
	renderPassBeginInfo.renderArea.extent = vulkanSwapChainExtent;
	VkClearValue clearValue = {{ {0.0f, 0.0f, 0.0f, 1.0f} }};  // Its drilling down 3 levels into nested unions to reach the array of floats
	renderPassBeginInfo.pClearValues = &clearValue;  // Clear values to use for VK_ATTACHMENT_LOAD_OP_CLEAR (black in this case)
	renderPassBeginInfo.clearValueCount = 1;

	// Begin the render pass (commands will be embedded in the Primary command buffer itself. No usage of secondary cmd buffers)
	vkCmdBeginRenderPass(vulkanCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	// Bind the Graphics Pipeline
	vkCmdBindPipeline(vulkanCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanGraphicsPipeline);

	// We specified viewport and scissor state for this pipeline to be dynamic. 
	// So we need to set them in the command buffer before issuing our draw command.
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	viewport.width = static_cast<float>(vulkanSwapChainExtent.width);
	viewport.height = static_cast<float>(vulkanSwapChainExtent.height);
	vkCmdSetViewport(vulkanCommandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0,0 };
	scissor.extent = vulkanSwapChainExtent;
	vkCmdSetScissor(vulkanCommandBuffer, 0, 1, &scissor);

	// Issue the Draw command for the Triangle
	vkCmdDraw(vulkanCommandBuffer, 3, 1, 0, 0);  // Use 1 for instanceCount if NOT using instanced rendering

	// End the Render Pass
	vkCmdEndRenderPass(vulkanCommandBuffer);

	// Finished recording the Command Buffer:
	result = vkEndCommandBuffer(vulkanCommandBuffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("RUNTIME ERROR: Failed to record Command Buffer!");
	}

}

/// @brief The render loop.
void Application::drawFrame() {

	// At the start of the frame, we want to wait until the previous frame has finished, 
	// so that the command buffer and semaphores are available to use.
	vkWaitForFences(vulkanLogicalDevice, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

	// After waiting, we need to manually reset the fence to the 'unisgnalled' state
	vkResetFences(vulkanLogicalDevice, 1, &inFlightFence);

	// Acquiring an image from the SwapChain
	uint32_t swapChainImageIndex{};
	VkResult result = vkAcquireNextImageKHR(vulkanLogicalDevice, vulkanSwapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &swapChainImageIndex);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("RUNTIME ERROR: Failed to acquire the next image from the swapchain!");
	}

	// Recording the Command Buffer
	vkResetCommandBuffer(vulkanCommandBuffer, 0);
	recordCommandBuffer(vulkanCommandBuffer, swapChainImageIndex);

	// Submit the command buffer:
	VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };  // wait semaphores
	VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };  // signal semaphores
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };  // pipeline wait stages

	VkSubmitInfo commandBufferSubmitInfo{};  // command submit info
	commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	commandBufferSubmitInfo.waitSemaphoreCount = 1;
	commandBufferSubmitInfo.signalSemaphoreCount = 1;
	commandBufferSubmitInfo.pWaitSemaphores = waitSemaphores;
	commandBufferSubmitInfo.pWaitDstStageMask = waitStages;
	commandBufferSubmitInfo.pSignalSemaphores = signalSemaphores;
	commandBufferSubmitInfo.pCommandBuffers = &vulkanCommandBuffer;
	commandBufferSubmitInfo.commandBufferCount = 1;

	result = vkQueueSubmit(deviceGraphicsQueue, 1, &commandBufferSubmitInfo, inFlightFence);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("RUNTIME ERROR: Failed to submit draw command buffer to graphics queue!");
	}

	// Presentation
	VkSwapchainKHR swapChains[] = { vulkanSwapChain };

	VkPresentInfoKHR presentationInfo{};
	presentationInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentationInfo.pWaitSemaphores = signalSemaphores;
	presentationInfo.waitSemaphoreCount = 1;
	presentationInfo.pSwapchains = swapChains;
	presentationInfo.swapchainCount = 1;  // Will almost always be only 1
	presentationInfo.pImageIndices = &swapChainImageIndex;
	presentationInfo.pResults = nullptr; // optional: Allows specifying a VkResult array to check for success of presentation in each swapchain

	vkQueuePresentKHR(devicePresentationQueue, &presentationInfo);

}

void Application::createSynchronizationObjects() {

	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	
	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Creates the fence initialized to the signaled state (for the first drawFrame call)

	// Create the Synchronization Objects
	if (vkCreateSemaphore(vulkanLogicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS) {
		throw std::runtime_error("RUNTIME ERROR: Failed to create imageAvailableSemaphore!");
	}
#ifdef NDEBUG 
	// Release Mode:
#else         
	// Debug Mode:
	std::cout << "DEBUG LOG: Created Vulkan synchronization object (semaphore): 'imageAvailableSemaphore' successfully.\n";
#endif	

	if (vkCreateSemaphore(vulkanLogicalDevice, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS) {
		throw std::runtime_error("RUNTIME ERROR: Failed to create renderFinishedSemaphore!");
	}
#ifdef NDEBUG 
// Release Mode:
#else         
	// Debug Mode:
	std::cout << "DEBUG LOG: Created Vulkan synchronization object (semaphore): 'renderFinishedSemaphore' successfully.\n";
#endif

	if (vkCreateFence(vulkanLogicalDevice, &fenceCreateInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
		throw std::runtime_error("RUNTIME ERROR: Failed to create inFlightFence!");
	}
#ifdef NDEBUG 
// Release Mode:
#else         
	// Debug Mode:
	std::cout << "DEBUG LOG: Created Vulkan synchronization object (fence): 'inFlightFence' successfully.\n";
#endif

	std::cout << "> Created Vulkan synchronization objects successfully.\n";

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
