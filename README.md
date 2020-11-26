# vulkan-triangle
Following Vulkan SDK tutorial from https://vulkan-tutorial.com/Introduction

## Setup
### Install Prerequisites
Vulkan packages:
`sudo apt install vulkan-tools`: Command-line utilities, most importantly `vulkaninfo` and `vkcube`. Run these to confirm your machine supports Vulkan.
`sudo apt install libvulkan-dev`: Installs Vulkan loader. The loader looks up the functions in the driver at runtime, similarly to GLEW for OpenGL - if you're familiar with that.
`sudo apt install vulkan-validationlayers-dev`: Installs the standard validation layers. These are crucial when debugging Vulkan applications.

GLFW:
`sudo apt install libglfw3-dev`: GLFW provides window creation while abstracting some platform-specific things.

GLM:
`sudo apt install libglm-dev`
