VULKAN_SDK_PATH = ~/VulkanSDK/1.2.148.1/x86_64

CFLAGS = -std=c++17 -I$(VULKAN_SDK_PATH)/include

LDFLAGS = -L$(VULKAN_SDK_PATH)/lib `pkg-config --static --libs glfw3` -lvulkan

VulkanTriangle: main.cpp
	g++ $(CFLAGS) -o VulkanTriangle main.cpp $(LDFLAGS)

.PHONY: test clean

test: VulkanTriangle
	LD_LIBRARY_PATH=$(VULKAN_SDK_PATH)/lib VK_LAYER_PATH=$(VULKAN_SDK_PATH)/etc/vulkan/explicit_layer.d ./VulkanTriangle

clean:
	rm -f VulkanTriangle
