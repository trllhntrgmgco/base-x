#include <iostream>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "BaseX/Engine.h"

int main() {
    std::cout << "Graphical Demo with GLFW and Vulkan\n";

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    // Vulkan instance
    VkInstance instance;
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Graphical Demo";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "BaseX";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance!" << std::endl;
        glfwTerminate();
        return 1;
    }

    std::cout << "Vulkan instance created\n";

    GLFWwindow* window = glfwCreateWindow(800, 600, "BaseX Demo", nullptr, nullptr);
    if (!window) {
        vkDestroyInstance(instance, nullptr);
        glfwTerminate();
        return 1;
    }

    basex::Engine engine;
    if (!engine.initialize()) {
        std::cerr << "Failed to initialize engine.\n";
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    while (!glfwWindowShouldClose(window) && engine.isRunning()) {
        glfwPollEvents();
        engine.update(0.016f);
        // Vulkan rendering would go here
        std::cout << "Rendering with Vulkan\n";
    }

    engine.shutdown();
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}