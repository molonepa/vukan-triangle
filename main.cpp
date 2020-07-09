#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <set>
#include <algorithm>

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
		const uint32_t WIDTH = 800;
		const uint32_t HEIGHT = 600;

		GLFWwindow* window;
		VkSurfaceKHR surface;

		VkInstance instance;

		VkDebugUtilsMessengerEXT debugMessenger;

		VkSwapchainKHR swapchain;

		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkDevice logicalDevice;

		VkQueue graphicsQueue;
		VkQueue presentQueue;

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

			// query surface formats supported by specified device
			uint32_t formatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

			if (formatCount != 0) {
				details.formats.resize(formatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
			}

			// query present modes supported by specified device
			uint32_t presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

			if (formatCount != 0) {
				details.presentModes.resize(presentModeCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
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
		}

		void mainLoop() {
			while (!glfwWindowShouldClose(window)) {
				glfwPollEvents();
			}
		}

		void cleanup() {
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
