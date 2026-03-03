// Rendering implementation using Vulkan

#include <vulkan/vulkan.h>
#include <iostream>

namespace basex {

VkInstance createVulkanInstance() {
    VkInstance instance;
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "BaseX Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "BaseX";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance!" << std::endl;
        return VK_NULL_HANDLE;
    }

    std::cout << "Vulkan instance created." << std::endl;
    return instance;
}

void destroyVulkanInstance(VkInstance instance) {
    vkDestroyInstance(instance, nullptr);
    std::cout << "Vulkan instance destroyed." << std::endl;
}

} // namespace basex
