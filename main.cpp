#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <set>
#include <algorithm>

const int MAX_FRAMES_IN_FLIGHT = 2;

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};
const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef NDEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

static std::vector<char> readFile(const std::string& filename) {
	// begin at end of file to get size
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("ERROR: Failed to open file " + filename);
	}

	size_t fileSize = (size_t) file.tellg();
	//std::cout << filename + " " + std::to_string(fileSize) << std::endl;

	std::vector<char> buffer(fileSize);

	// read from beginning of file
	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

class VulkanTriangleApplication {
	public:
		void run() {
			initWindow();
			initVulkan();
			mainLoop();
			cleanup();
		}

	private:
		GLFWwindow* window;
		VkSurfaceKHR surface;

		VkInstance instance;

		VkDebugUtilsMessengerEXT debugMessenger;

		VkSwapchainKHR swapchain;
		std::vector<VkImage> swapchainImages;
		std::vector<VkImageView> swapchainImageViews;
		std::vector<VkFramebuffer> swapchainFramebuffers;
		VkFormat swapchainImageFormat;
		VkExtent2D swapchainExtent;

		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkDevice logicalDevice;

		VkQueue graphicsQueue;
		VkQueue presentQueue;

		VkRenderPass renderPass;
		VkPipelineLayout pipelineLayout;
		VkPipeline graphicsPipeline;

		VkCommandPool commandPool;
		std::vector<VkCommandBuffer> commandBuffers;

		std::vector<VkSemaphore> imageAvailableSemaphores;
		std::vector<VkSemaphore> renderFinishedSemaphores;
		std::vector<VkFence> inFlightFences;
		std::vector<VkFence> imagesInFlight;
		size_t currentFrame = 0;

		void initWindow() {
			glfwInit();

			// do not create OpenGL context
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			// disable window resizing
			glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

			// create WIDTH * HEIGHT sized window in windowed mode
			window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Triangle", nullptr, nullptr);
		}

		void initVulkan() {
			createInstance();
			setupDebugMessenger();
			createSurface();
			choosePhysicalDevice();
			createLogicalDevice();
			createSwapChain();
			createImageViews();
			createRenderPass();
			createGraphicsPipeline();
			createFramebuffers();
			createCommandPool();
			createCommandBuffers();
			createSyncObjects();
		}

		void createInstance() {
			if (enableValidationLayers && !checkValidationLayerSupport()) {
				throw std::runtime_error("ERROR: Required validation layer(s) not available");
			}

			if (!checkInstanceExtensionSupport()) {
				throw std::runtime_error("ERROR: Required extension(s) not available");
			}

			// optional struct providing parameters to optimize application
			VkApplicationInfo appInfo{};
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pApplicationName = "Vulkan Triangle";
			appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.pEngineName = "No engine";
			appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.apiVersion = VK_API_VERSION_1_0;

			// required struct for instance creation
			VkInstanceCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			createInfo.pApplicationInfo = &appInfo;

			uint32_t glfwExtensionCount = 0;
			const char** glfwExtensions;

			// pass required extensions
			std::vector<const char*> requiredExtensions = getRequiredExtensions();
			createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
			createInfo.ppEnabledExtensionNames = requiredExtensions.data();

			// pass validation layer names if enabled
			VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
			if (enableValidationLayers) {
				createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
				createInfo.ppEnabledLayerNames = validationLayers.data();

				// pass pointer to debuggerCreateInfo to allow debugging instance creation and destruction
				populateDebugMessengerCreateInfo(debugCreateInfo);
				createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
			}
			else {
				createInfo.enabledLayerCount = 0;
				createInfo.pNext = nullptr;
			}

			// create instance with specified parameters and no custom memory allocator callback
			if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
				throw std::runtime_error("ERROR: Failed to create instance");
			}
		}

		std::vector<const char*> getRequiredExtensions() {
			uint32_t glfwExtensionCount = 0;
			const char** glfwExtensions;

			//get extensions required to interface with GLFW window
			glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

			std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

			if (enableValidationLayers) {
				extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			}

			return extensions;
		}

		bool checkInstanceExtensionSupport() {
			std::vector<const char*> requiredExtensions = getRequiredExtensions();

			uint32_t availableExtensionCount = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);

			std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
			vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensions.data());

			for (const auto& required : requiredExtensions) {
				bool extensionFound = false;
				for (const auto& available : availableExtensions) {
					if (strcmp(required, available.extensionName) == 0) {
						extensionFound = true;
						break;
					}
				}

				if (!extensionFound) {
					std::cout << "Required extensions:" << std::endl;
					for (const auto& required : requiredExtensions) {
						std::cout << "\t" << required << std::endl;
					}

					std::cout << "Available extensions:" << std::endl;
					for (const auto& available : availableExtensions) {
						std::cout << "\t" << available.extensionName << std::endl;
					}

					return false;
				}
			}
			return true;
		}

		bool checkValidationLayerSupport() {
			uint32_t layerCount;
			vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

			std::vector<VkLayerProperties> availableLayers(layerCount);
			vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

			for (const char* layerName : validationLayers) {
				bool layerFound = false;
				for (const auto& layerProperties : availableLayers) {
					if (strcmp(layerName, layerProperties.layerName) == 0) {
						layerFound = true;
						break;
					}
				}

				if (!layerFound) {
					return false;
				}
			}
			return true;
		}

		static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallBack(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
			std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;

			return VK_FALSE;
		}

		void setupDebugMessenger() {
			if (!enableValidationLayers) {
				return;
			}

			VkDebugUtilsMessengerCreateInfoEXT createInfo;
			populateDebugMessengerCreateInfo(createInfo);

			if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
				throw std::runtime_error("ERROR: Failed to set up debug messenger");
			}
		}

		void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
			createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			//createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT; // ignore verbose general info
			createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT; // only care about warnigs and errors
			createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			createInfo.pfnUserCallback = debugCallBack;
			createInfo.pUserData = nullptr; // optional
		}

		void choosePhysicalDevice() {
			uint32_t deviceCount = 0;
			vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

			if (deviceCount == 0) {
				throw std::runtime_error("ERROR: Failed to find device with Vulkan support");
			}

			// query devices
			std::vector<VkPhysicalDevice> devices(deviceCount);
			vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

			// find suitable device
			for (const auto& device : devices) {
				if (isDeviceSuitable(device)) {
					physicalDevice = device;
					break;
				}
			}

			if (physicalDevice == VK_NULL_HANDLE) {
				throw std::runtime_error("ERROR: Failed to find suitable device");
			}
		}

		bool isDeviceSuitable(VkPhysicalDevice device) {
			// get basic device properties
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(device, &deviceProperties);

			// get queue families supported by device
			QueueFamilyIndices indices = findQueueFamilies(device);

			if (!indices.isComplete()) {
				throw std::runtime_error("ERROR: Required queue families not supported by device");
			}

			// check whether device supports necessary extensions (e.g. VK_KHR_swapchain extension)
			bool extensionsSupported = checkDeviceExtensionSupport(device);

			if (!extensionsSupported) {
				throw std::runtime_error("ERROR: Required device extension(s) not supported by device");
			}

			// check whether device has swapchain support appropriate for surface being used
			bool swapChainAdequate = false;

			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();

			if (!swapChainAdequate) {
				throw std::runtime_error("ERROR: Required swapchain extension(s) not supported by device");
			}

			// in this case return true if device is dedicated GPU and passes other checks
			return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && indices.isComplete() && extensionsSupported && swapChainAdequate;
		}

		bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
			uint32_t extensionCount;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

			std::vector<VkExtensionProperties> availableExtensions(extensionCount);
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

			std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

			for (const auto& extension : availableExtensions) {
				requiredExtensions.erase(extension.extensionName);
			}

			return requiredExtensions.empty();
		}

		QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
			QueueFamilyIndices indices;

			uint32_t queueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

			// query queue family types supported by specified device
			std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

			// find at least one queue family that supports VK_QUEUE_GRAPHICS_BIT and WSI
			int i = 0;
			for (const auto& queueFamily : queueFamilies) {
				if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
					indices.graphicsFamily = i;
				}

				VkBool32 presentSupport = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

				if (presentSupport) {
					indices.presentFamily = i;
				}

				if (indices.isComplete()) {
					break;
				}

				i++;
			}

			return indices;
		}

		SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
			SwapChainSupportDetails details;

			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

			// query surface formats supported by specified device
			uint32_t formatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

			if (formatCount != 0) {
				details.formats.resize(formatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
			}
			else {
				throw std::runtime_error("ERROR: Failed to retrieve surface format details");
			}

			// query present modes supported by specified device
			uint32_t presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

			if (presentModeCount != 0) {
				details.presentModes.resize(presentModeCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
			}
			else {
				throw std::runtime_error("ERROR: Failed to retrieve present mode details");
			}

			return details;
		}

		void createLogicalDevice() {
			QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

			std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
			std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

			// populate queue creation struct
			float queuePriority = 1.0f;
			for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
				queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueCreateInfo.queueFamilyIndex = queueFamily;
				queueCreateInfo.queueCount = 1;
				queueCreateInfo.pQueuePriorities = &queuePriority;
				queueCreateInfos.push_back(queueCreateInfo);
			}

			// no specific device features are required for now
			VkPhysicalDeviceFeatures deviceFeatures{};

			// popuate logical device creation struct
			VkDeviceCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

			createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
			createInfo.pQueueCreateInfos = queueCreateInfos.data();

			createInfo.pEnabledFeatures = &deviceFeatures;

			// push required device extensions
			createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
			createInfo.ppEnabledExtensionNames = deviceExtensions.data();

			if (enableValidationLayers) {
				createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
				createInfo.ppEnabledLayerNames = validationLayers.data();
			}
			else {
				createInfo.enabledLayerCount = 0;
			}

			if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice) != VK_SUCCESS) {
				throw std::runtime_error("ERROR: Failed to create logical device");
			}

			// get queue handles
			vkGetDeviceQueue(logicalDevice, indices.graphicsFamily.value(), 0, &graphicsQueue);
			vkGetDeviceQueue(logicalDevice, indices.presentFamily.value(), 0, &presentQueue);
		}

		void createSurface() {
			// use GLFW to avoid platform specific window surface creation
			if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
				throw std::runtime_error("ERROR: Failed to create window surface");
			}
		}

		VkSurfaceFormatKHR chooseSwapchainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
			// check available formats for one that supports SRGB
			for (const auto& availableFormat : availableFormats) {
				if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
					return availableFormat;
				}
			}

			return availableFormats[0]; // return first available format if none support SRGB
		}

		VkPresentModeKHR chooseSwapchainPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
			// check available present modes for one that supports Mailbox, which can be used to implement triple buffering
			for (const auto& availablePresentMode : availablePresentModes) {
				if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
					return availablePresentMode;
				}
			}

			return VK_PRESENT_MODE_FIFO_KHR; // use FIFO if Mailbox unavailable
		}

		VkExtent2D chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
			if (capabilities.currentExtent.width != UINT32_MAX) {
				return capabilities.currentExtent;
			}
			else {
				VkExtent2D actualExtent = {WIDTH, HEIGHT};

				// clamp extent width and height to values supported by surface
				actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
				actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

				return actualExtent;
			}
		}

		void createSwapChain() {
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

			VkSurfaceFormatKHR surfaceFormat = chooseSwapchainSurfaceFormat(swapChainSupport.formats);
			VkPresentModeKHR presentMode = chooseSwapchainPresentMode(swapChainSupport.presentModes);
			VkExtent2D extent = chooseSwapchainExtent(swapChainSupport.capabilities);

			// store image format and extent in member variables for future use
			swapchainImageFormat = surfaceFormat.format;
			swapchainExtent = extent;

			// set swapchain image count to supported minimum + 1 to avoid stalling and ensure it doesn't exceed maximum
			uint32_t imageCount = swapChainSupport.capabilities.minImageCount +1;

			if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
				imageCount = swapChainSupport.capabilities.maxImageCount;
			}

			// populate swapchain creation struct with specified values
			VkSwapchainCreateInfoKHR createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			createInfo.surface = surface;

			createInfo.minImageCount = imageCount;
			createInfo.imageFormat = surfaceFormat.format;
			createInfo.imageColorSpace = surfaceFormat.colorSpace;
			createInfo.imageExtent = extent;
			createInfo.imageArrayLayers = 1; // always 1 unless developing stereoscopic 3D application
			createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

			QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
			uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

			if (indices.graphicsFamily != indices.presentFamily) {
				// images need not be explicitly transferred between queue families
				createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
				createInfo.queueFamilyIndexCount = 2;
				createInfo.pQueueFamilyIndices = queueFamilyIndices;
			}
			else {
				// images are owned by one queue family at a time and must be transferred explicitly
				createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
				createInfo.queueFamilyIndexCount = 0; // optional
				createInfo.pQueueFamilyIndices = nullptr; // optional
			}

			// transforms (e.g. rotation, flipping) can be applied to images in the swapchain, currentTransform specifies no transform
			createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

			createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // ignore alpha channel

			createInfo.presentMode = presentMode;

			createInfo.clipped = VK_TRUE;
			createInfo.oldSwapchain = VK_NULL_HANDLE;

			if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
				throw std::runtime_error("ERROR: Failed to create swapchain");
			}

			// retrieve images from swapchain and store in vector
			vkGetSwapchainImagesKHR(logicalDevice, swapchain, &imageCount, nullptr);
			swapchainImages.resize(imageCount);
			vkGetSwapchainImagesKHR(logicalDevice, swapchain, &imageCount, swapchainImages.data());
		}

		void createImageViews() {
			swapchainImageViews.resize(swapchainImages.size());

			// create an image view for every image in the swapchain
			for (size_t i = 0; i < swapchainImages.size(); i++) {
				VkImageViewCreateInfo createInfo{};

				createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				createInfo.image = swapchainImages[i];

				createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				createInfo.format = swapchainImageFormat;

				createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

				createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				createInfo.subresourceRange.baseMipLevel = 0;
				createInfo.subresourceRange.levelCount = 1;
				createInfo.subresourceRange.baseArrayLayer = 0;
				createInfo.subresourceRange.layerCount = 1;

				if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
					throw std::runtime_error("ERROR: Failed to create image view");
				}
			}
		}

		void createGraphicsPipeline() {
			// load compiled shader bytecode
			auto vertexShaderCode = readFile("shaders/vert.spv");
			auto fragmentShaderCode = readFile("shaders/frag.spv");

			VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
			VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);

			// vertex shader stage creation
			VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo{};
			vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
			vertexShaderStageCreateInfo.module = vertexShaderModule;
			vertexShaderStageCreateInfo.pName = "main"; // entrypoint

			// fragment shader stage creation
			VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo{};
			fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			fragmentShaderStageCreateInfo.module = fragmentShaderModule;
			fragmentShaderStageCreateInfo.pName = "main"; // entrypoint

			VkPipelineShaderStageCreateInfo shaderStages[] = {vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo};

			// vertex data is defined in the shader for now so no need to pass anything
			VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
			vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertexInputInfo.vertexBindingDescriptionCount = 0;
			vertexInputInfo.pVertexBindingDescriptions = nullptr;
			vertexInputInfo.vertexAttributeDescriptionCount = 0;
			vertexInputInfo.pVertexAttributeDescriptions = nullptr;

			// input assembly configuration
			VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
			inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

			// viewport configuration
			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = (float) swapchainExtent.width;
			viewport.height = (float) swapchainExtent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			// draw entire framebuffer to viewport
			VkRect2D scissor{};
			scissor.offset = {0, 0};
			scissor.extent = swapchainExtent;

			VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
			viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportStateCreateInfo.viewportCount = 1;
			viewportStateCreateInfo.pViewports = &viewport;
			viewportStateCreateInfo.scissorCount = 1;
			viewportStateCreateInfo.pScissors = &scissor;

			// rasterizer configuration
			VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo{};
			rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizerCreateInfo.depthClampEnable = VK_FALSE; // if VK_TRUE fragments beyond near and far planes are clamped instead of discarded
			rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE; // if VK_TRUE geometry never passes through rasterizer stage
			rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL; // fill each polygon with fragments
			rasterizerCreateInfo.lineWidth = 1.0f;
			rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
			rasterizerCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
			rasterizerCreateInfo.depthBiasEnable = VK_FALSE;
			rasterizerCreateInfo.depthBiasConstantFactor = 0.0f;
			rasterizerCreateInfo.depthBiasClamp = 0.0f;
			rasterizerCreateInfo.depthBiasSlopeFactor = 0.0f;

			// multisampling configuration
			VkPipelineMultisampleStateCreateInfo multiSamplingCreateInfo{};
			multiSamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multiSamplingCreateInfo.sampleShadingEnable = VK_FALSE;
			multiSamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			multiSamplingCreateInfo.minSampleShading = 1.0f;
			multiSamplingCreateInfo.pSampleMask = nullptr;
			multiSamplingCreateInfo.alphaToCoverageEnable = VK_FALSE;
			multiSamplingCreateInfo.alphaToOneEnable = VK_FALSE;

			// colour blending configuration
			VkPipelineColorBlendAttachmentState colourBlendAttachment{};
			colourBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colourBlendAttachment.blendEnable = VK_FALSE;
			colourBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			colourBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
			colourBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
			colourBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colourBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colourBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

			VkPipelineColorBlendStateCreateInfo colourBlendCreateInfo{};
			colourBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colourBlendCreateInfo.logicOpEnable = VK_FALSE;
			colourBlendCreateInfo.logicOp = VK_LOGIC_OP_COPY;
			colourBlendCreateInfo.attachmentCount = 1;
			colourBlendCreateInfo.pAttachments = &colourBlendAttachment;
			colourBlendCreateInfo.blendConstants[0] = 0.0f;
			colourBlendCreateInfo.blendConstants[1] = 0.0f;
			colourBlendCreateInfo.blendConstants[2] = 0.0f;
			colourBlendCreateInfo.blendConstants[3] = 0.0f;

			// dynamic state configuration
			//VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH}; // some states of the pipeline can be dynamically changed without creating a new pipeline (e.g. viewport size, line width)
			//VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
			//dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			//dynamicStateCreateInfo.dynamicStateCount = 2;
			//dynamicStateCreateInfo.pDynamicStates = dynamicStates;

			// pipeline layout creation
			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
			pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCreateInfo.setLayoutCount = 0;
			pipelineLayoutCreateInfo.pSetLayouts = nullptr;
			pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
			pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

			if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
				throw std::runtime_error("ERROR: Failed to create graphics pipeline layout");
			}

			// pipeline creation
			VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
			graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			graphicsPipelineCreateInfo.stageCount = 2;
			graphicsPipelineCreateInfo.pStages = shaderStages;

			graphicsPipelineCreateInfo.pVertexInputState = &vertexInputInfo;
			graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
			graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
			graphicsPipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
			graphicsPipelineCreateInfo.pMultisampleState = &multiSamplingCreateInfo;
			graphicsPipelineCreateInfo.pDepthStencilState = nullptr;
			graphicsPipelineCreateInfo.pColorBlendState = &colourBlendCreateInfo;
			graphicsPipelineCreateInfo.pDynamicState = nullptr;
			//graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;

			graphicsPipelineCreateInfo.layout = pipelineLayout;

			graphicsPipelineCreateInfo.renderPass = renderPass;
			graphicsPipelineCreateInfo.subpass = 0;

			graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
			graphicsPipelineCreateInfo.basePipelineIndex = -1;

			if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
				throw std::runtime_error("ERROR: Failed to create graphics pipeline");
			}

			// clean up shader module objects
			vkDestroyShaderModule(logicalDevice, fragmentShaderModule, nullptr);
			vkDestroyShaderModule(logicalDevice, vertexShaderModule, nullptr);
		}

		VkShaderModule createShaderModule(const std::vector<char>& code) {
			// populate shader module creation struct
			VkShaderModuleCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = code.size();
			createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

			VkShaderModule shaderModule;

			if (vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
				throw std::runtime_error("ERROR: Failed to create shader module");
			}

			return shaderModule;
		}

		void createRenderPass() {
			VkAttachmentDescription colourAttachment{};
			colourAttachment.format = swapchainImageFormat;
			colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

			colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // clear framebuffer to black before drawing new frame
			colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

			colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			VkAttachmentReference colourAttachmentRef{};
			colourAttachmentRef.attachment = 0;
			colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass{};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colourAttachmentRef;

			VkSubpassDependency dependency{};
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.dstSubpass = 0;
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.srcAccessMask = 0;
			dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			VkRenderPassCreateInfo renderPassCreateInfo{};
			renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassCreateInfo.attachmentCount = 1;
			renderPassCreateInfo.pAttachments = &colourAttachment;
			renderPassCreateInfo.subpassCount = 1;
			renderPassCreateInfo.pSubpasses = &subpass;
			renderPassCreateInfo.dependencyCount = 1;
			renderPassCreateInfo.pDependencies = &dependency;

			if (vkCreateRenderPass(logicalDevice, &renderPassCreateInfo, nullptr, &renderPass) != VK_SUCCESS) {
				throw std::runtime_error("ERROR: Failed to create render pass");
			}
		}

		void createFramebuffers() {
			swapchainFramebuffers.resize(swapchainImageViews.size());

			// create a framebuffer for each image view
			for (size_t i = 0; i < swapchainImageViews.size(); i++) {
				VkImageView attachments[] = {swapchainImageViews[i]};

				VkFramebufferCreateInfo framebufferCreateInfo{};
				framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				framebufferCreateInfo.renderPass = renderPass;
				framebufferCreateInfo.attachmentCount = 1;
				framebufferCreateInfo.pAttachments = attachments;
				framebufferCreateInfo.width = swapchainExtent.width;
				framebufferCreateInfo.height = swapchainExtent.height;
				framebufferCreateInfo.layers = 1;

				if (vkCreateFramebuffer(logicalDevice, &framebufferCreateInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS) {
					throw std::runtime_error("ERROR: Failed to create framebuffer");
				}
			}
		}

		void createCommandPool() {
			QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

			VkCommandPoolCreateInfo commandPoolCreateInfo{};
			commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
			commandPoolCreateInfo.flags = 0;

			if (vkCreateCommandPool(logicalDevice, &commandPoolCreateInfo, nullptr, &commandPool) != VK_SUCCESS) {
				throw std::runtime_error("ERROR: Failed to create command pool");
			}
		}

		void createCommandBuffers() {
			commandBuffers.resize(swapchainFramebuffers.size());

			VkCommandBufferAllocateInfo allocateInfo{};
			allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocateInfo.commandPool = commandPool;
			allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocateInfo.commandBufferCount = (uint32_t) commandBuffers.size();

			if (vkAllocateCommandBuffers(logicalDevice, &allocateInfo, commandBuffers.data()) != VK_SUCCESS) {
				throw std::runtime_error("ERROR: Failed to allocate command buffers");
			}

			for (size_t i = 0; i < commandBuffers.size(); i++) {
				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = 0;
				beginInfo.pInheritanceInfo = nullptr;

				if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS) {
					throw std::runtime_error("ERROR: Failed to begin recording command buffer");
				}

				VkRenderPassBeginInfo renderPassInfo{};
				renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassInfo.renderPass = renderPass;
				renderPassInfo.framebuffer = swapchainFramebuffers[i];

				renderPassInfo.renderArea.offset = {0, 0};
				renderPassInfo.renderArea.extent = swapchainExtent;

				VkClearValue clearColour = {0.3f, 0.5f, 0.8f, 1.0f};
				renderPassInfo.clearValueCount = 1;
				renderPassInfo.pClearValues = &clearColour;

				vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

				vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

				vkCmdEndRenderPass(commandBuffers[i]);

				if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
					throw std::runtime_error("ERROR: Failed to record command buffer");
				}
			}
		}

		void createSyncObjects() {
			imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
			renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
			inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
			imagesInFlight.resize(swapchainImages.size(), VK_NULL_HANDLE);

			VkSemaphoreCreateInfo semaphoreCreateInfo{};
			semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			VkFenceCreateInfo fenceInfo{};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				if (vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS || vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS || vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
					throw std::runtime_error("ERROR: Failed to create synchronisation objects");
				}
			}
		}

		void mainLoop() {
			while (!glfwWindowShouldClose(window)) {
				glfwPollEvents();
				drawFrame();
			}
			vkDeviceWaitIdle(logicalDevice);
		}

		void drawFrame() {
			vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

			uint32_t imageIndex;

			// retrieve image from swapchain
			vkAcquireNextImageKHR(logicalDevice, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

			// check if a previous frame is using this image
			if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
				vkWaitForFences(logicalDevice, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
			}

			// mark the image as being in use by this frame
			imagesInFlight[imageIndex] = inFlightFences[currentFrame];

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

			VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
			VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;

			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

			VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = signalSemaphores;

			vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);

			if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
				throw std::runtime_error("ERROR: Failed to submit draw coimmand buffer");
			}

			VkPresentInfoKHR presentInfo{};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = signalSemaphores;

			VkSwapchainKHR swapchains[] = {swapchain};
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = swapchains;
			presentInfo.pImageIndices = &imageIndex;
			presentInfo.pResults = nullptr;

			vkQueuePresentKHR(presentQueue, &presentInfo);

			currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
		}

		void cleanup() {
			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
				vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
				vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
			}

			vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

			for (auto framebuffer : swapchainFramebuffers) {
				vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
			}

			vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);

			vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);

			vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

			for (auto imageView : swapchainImageViews) {
				vkDestroyImageView(logicalDevice, imageView, nullptr);
			}

			vkDestroySwapchainKHR(logicalDevice, swapchain, nullptr);

			vkDestroyDevice(logicalDevice, nullptr);

			if (enableValidationLayers) {
				DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
			}

			vkDestroySurfaceKHR(instance, surface, nullptr);

			vkDestroyInstance(instance, nullptr);

			glfwDestroyWindow(window);

			glfwTerminate();
		}
};

int main() {
	VulkanTriangleApplication app;

	try {
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
