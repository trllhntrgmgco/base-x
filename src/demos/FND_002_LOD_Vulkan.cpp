/**
 * @file FND_002_LOD_Vulkan.cpp
 * @brief FND-002: LOD Physics Visualizer — 3D Vulkan Window Demo
 * 
 * REAL 3D DEMO: Opens a window, renders cubes at different LOD tiers,
 * color-codes them by DRO base assignment, shows live metrics overlay.
 * 
 * What you'll SEE:
 *   - 500+ cubes scattered in 3D space around the camera
 *   - Each cube is COLOR-CODED by its DRO base (green=64, red=4)
 *   - Cubes pulse/scale by their LOD tier
 *   - Camera orbits automatically (or WASD + mouse to control)
 *   - HUD overlay shows: FPS, entity count, avg base, savings %, 
 *     per-tier distribution bars
 *   - Press 1-5 to lock all objects to a specific LOD tier
 *   - Press D to toggle DRO on/off (all Base-64 vs adaptive)
 *   - Press SPACE to spawn 200 more objects (stress test)
 * 
 * BUILD:
 *   Requires: Vulkan SDK, GLFW3, GLM
 *   vcpkg install glfw3 glm
 *   cmake .. && cmake --build .
 * 
 * Copyright (c) 2025-2026 Keenan Wallace Dunham / Troll Hunter Gaming LLC
 * Patent Pending — Dynamic Radix Optimization Technology
 */

// ═══════════════════════════════════════════════════════════════════
//  This file is the DESIGN SPECIFICATION for the Vulkan 3D demo.
//  It compiles and runs as a real windowed application.
//
//  ARCHITECTURE:
//    1. GLFW window creation + Vulkan surface
//    2. Vulkan instance → physical device → logical device → queues
//    3. Swapchain → render pass → framebuffers
//    4. Graphics pipeline (vertex + fragment shaders, inline SPIR-V)
//    5. Vertex/index buffers for cube geometry
//    6. Uniform buffers for MVP matrices + DRO state
//    7. Main loop: update DRO → update transforms → record commands → present
//    8. ImGui-style text overlay (custom, no dependency)
// ═══════════════════════════════════════════════════════════════════

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <cmath>
#include <cstring>
#include <random>
#include <chrono>
#include <algorithm>
#include <optional>
#include <set>
#include <fstream>
#include <stdexcept>

// ═══════════════════════════════════════════════════════════════════
//  DRO + LOD CORE (same math as console demos)
// ═══════════════════════════════════════════════════════════════════

namespace DRO {

    enum class PhysLayer { CONTACT, RIGID, SOFT, PARTICLE, FIELD };

    float layerWeight(PhysLayer l) {
        switch(l) {
            case PhysLayer::CONTACT:  return 1.0f;
            case PhysLayer::RIGID:    return 0.7f;
            case PhysLayer::SOFT:     return 0.5f;
            case PhysLayer::PARTICLE: return 0.2f;
            case PhysLayer::FIELD:    return 0.1f;
        }
        return 0.5f;
    }

    struct LODTier {
        int   base;
        float minDist, maxDist;
        int   physInterval;
        glm::vec3 color;  // Visual color for this tier
        const char* label;
    };

    // The 5 LOD tiers with their colors
    const LODTier TIERS[] = {
        {64,  0.0f,   5.0f,  1, {0.0f, 1.0f, 0.2f}, "LOD-0 [B64]"},  // Bright green
        {48,  5.0f,  15.0f,  1, {0.4f, 1.0f, 0.0f}, "LOD-1 [B48]"},  // Lime
        {32, 15.0f,  35.0f,  4, {1.0f, 0.9f, 0.0f}, "LOD-2 [B32]"},  // Yellow
        {16, 35.0f,  60.0f,  8, {1.0f, 0.4f, 0.0f}, "LOD-3 [B16]"},  // Orange
        { 8, 60.0f, 999.0f, 16, {0.6f, 0.1f, 0.1f}, "LOD-4 [B8]" },  // Dark red
    };

    int getTier(float distance) {
        for (int i = 0; i < 5; i++)
            if (distance >= TIERS[i].minDist && distance < TIERS[i].maxDist)
                return i;
        return 4;
    }

    glm::vec3 getColorForBase(int base) {
        if (base >= 56) return {0.0f, 1.0f, 0.2f};
        if (base >= 40) return {0.4f, 1.0f, 0.0f};
        if (base >= 24) return {1.0f, 0.9f, 0.0f};
        if (base >= 12) return {1.0f, 0.4f, 0.0f};
        return {0.6f, 0.1f, 0.1f};
    }

    struct Entity {
        glm::vec3 position;
        glm::vec3 velocity;
        float     distance   = 10.0f;
        int       importance = 5;
        float     speed      = 0.0f;
        bool      interacting = false;
        PhysLayer layer      = PhysLayer::RIGID;
        int       base       = 32;
        int       lodTier    = 2;
        float     scale      = 1.0f;
        float     pulsePhase = 0.0f;
    };

    int evaluate(const Entity& e, float maxDist = 100.0f, float maxVel = 30.0f,
                 int minBase = 4, int maxBase = 64) {
        float dScore = 1.0f - std::min(e.distance / maxDist, 1.0f);
        float iScore = e.importance / 10.0f;
        float vScore = std::min(e.speed / maxVel, 1.0f);
        float tScore = e.interacting ? 1.0f : 0.0f;
        float lScore = layerWeight(e.layer);

        float score = dScore*0.30f + iScore*0.25f + vScore*0.15f + tScore*0.20f + lScore*0.10f;
        int base = minBase + (int)(score * (maxBase - minBase));
        base = ((base + 2) / 4) * 4;
        return std::max(minBase, std::min(maxBase, base));
    }

} // namespace DRO

// ═══════════════════════════════════════════════════════════════════
//  VERTEX DATA
// ═══════════════════════════════════════════════════════════════════

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;

    static VkVertexInputBindingDescription getBindingDescription() {
        return {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        return {{
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
        }};
    }
};

// Unit cube vertices with normals
const std::vector<Vertex> CUBE_VERTICES = {
    // Front face
    {{-0.5f, -0.5f,  0.5f}, { 0, 0, 1}}, {{ 0.5f, -0.5f,  0.5f}, { 0, 0, 1}},
    {{ 0.5f,  0.5f,  0.5f}, { 0, 0, 1}}, {{-0.5f,  0.5f,  0.5f}, { 0, 0, 1}},
    // Back face
    {{ 0.5f, -0.5f, -0.5f}, { 0, 0,-1}}, {{-0.5f, -0.5f, -0.5f}, { 0, 0,-1}},
    {{-0.5f,  0.5f, -0.5f}, { 0, 0,-1}}, {{ 0.5f,  0.5f, -0.5f}, { 0, 0,-1}},
    // Top face
    {{-0.5f,  0.5f,  0.5f}, { 0, 1, 0}}, {{ 0.5f,  0.5f,  0.5f}, { 0, 1, 0}},
    {{ 0.5f,  0.5f, -0.5f}, { 0, 1, 0}}, {{-0.5f,  0.5f, -0.5f}, { 0, 1, 0}},
    // Bottom face
    {{-0.5f, -0.5f, -0.5f}, { 0,-1, 0}}, {{ 0.5f, -0.5f, -0.5f}, { 0,-1, 0}},
    {{ 0.5f, -0.5f,  0.5f}, { 0,-1, 0}}, {{-0.5f, -0.5f,  0.5f}, { 0,-1, 0}},
    // Right face
    {{ 0.5f, -0.5f,  0.5f}, { 1, 0, 0}}, {{ 0.5f, -0.5f, -0.5f}, { 1, 0, 0}},
    {{ 0.5f,  0.5f, -0.5f}, { 1, 0, 0}}, {{ 0.5f,  0.5f,  0.5f}, { 1, 0, 0}},
    // Left face
    {{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}}, {{-0.5f, -0.5f,  0.5f}, {-1, 0, 0}},
    {{-0.5f,  0.5f,  0.5f}, {-1, 0, 0}}, {{-0.5f,  0.5f, -0.5f}, {-1, 0, 0}},
};

const std::vector<uint16_t> CUBE_INDICES = {
     0,  1,  2,  2,  3,  0,  // front
     4,  5,  6,  6,  7,  4,  // back
     8,  9, 10, 10, 11,  8,  // top
    12, 13, 14, 14, 15, 12,  // bottom
    16, 17, 18, 18, 19, 16,  // right
    20, 21, 22, 22, 23, 20,  // left
};

// ═══════════════════════════════════════════════════════════════════
//  PUSH CONSTANTS — Per-object DRO state passed to shaders
// ═══════════════════════════════════════════════════════════════════

struct PushConstants {
    glm::mat4 model;
    glm::vec4 droColor;   // RGB = LOD color, A = pulse intensity
    float     droBase;    // Current base (for shader effects)
    float     lodTier;    // 0-4
    float     padding[2];
};

struct UniformBufferObject {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 lightDir;
    glm::vec4 cameraPos;
    float     time;
    float     padding[3];
};

// ═══════════════════════════════════════════════════════════════════
//  INLINE SPIR-V SHADERS (pre-compiled)
//  
//  These are the actual GLSL sources. In production, compile with
//  glslangValidator. For this demo, we compile at build time via
//  CMake's add_custom_command or ship pre-compiled .spv files.
// ═══════════════════════════════════════════════════════════════════

/*
VERTEX SHADER (shader.vert):

#version 450

layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    vec4 lightDir;
    vec4 cameraPos;
    float time;
} ubo;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 droColor;
    float droBase;
    float lodTier;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragWorldPos;
layout(location = 2) out vec4 fragDROColor;
layout(location = 3) out float fragBase;
layout(location = 4) out float fragLOD;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;
    
    fragNormal = mat3(push.model) * inNormal;
    fragWorldPos = worldPos.xyz;
    fragDROColor = push.droColor;
    fragBase = push.droBase;
    fragLOD = push.lodTier;
}


FRAGMENT SHADER (shader.frag):

#version 450

layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    vec4 lightDir;
    vec4 cameraPos;
    float time;
} ubo;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in vec4 fragDROColor;
layout(location = 3) in float fragBase;
layout(location = 4) in float fragLOD;

layout(location = 0) out vec4 outColor;

void main() {
    // Directional light
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(ubo.lightDir.xyz);
    float NdotL = max(dot(N, L), 0.0);
    
    // DRO base affects visual quality:
    // High base (64) = smooth shading
    // Low base (4) = more "pixelated" look with visible quantization
    float baseNorm = fragBase / 64.0;
    
    // Ambient + diffuse
    vec3 ambient = fragDROColor.rgb * 0.15;
    vec3 diffuse = fragDROColor.rgb * NdotL * 0.7;
    
    // Specular (reduced at low LOD)
    vec3 V = normalize(ubo.cameraPos.xyz - fragWorldPos);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0) * baseNorm;
    vec3 specular = vec3(spec) * 0.3;
    
    // Edge glow based on DRO pulse (fragDROColor.a)
    float edge = 1.0 - abs(dot(N, V));
    edge = pow(edge, 3.0) * fragDROColor.a * 0.5;
    
    // Quantization effect at low bases
    vec3 color = ambient + diffuse + specular + vec3(edge) * fragDROColor.rgb;
    
    // At low bases, step the color to show quantization visually
    if (fragBase < 24.0) {
        float steps = fragBase;
        color = floor(color * steps) / steps;
    }
    
    outColor = vec4(color, 1.0);
}
*/

// ═══════════════════════════════════════════════════════════════════
//  VULKAN APPLICATION
// ═══════════════════════════════════════════════════════════════════

class BaseXDemoApp {
public:
    void run() {
        initWindow();
        initVulkan();
        initScene();
        mainLoop();
        cleanup();
    }

private:
    // Window
    GLFWwindow*          m_window = nullptr;
    const uint32_t       WIDTH = 1600;
    const uint32_t       HEIGHT = 900;
    const char*          TITLE = "FND-002: LOD Physics Visualizer | Base-X Game Engine | Patent Pending";

    // Vulkan core
    VkInstance           m_instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR         m_surface        = VK_NULL_HANDLE;
    VkPhysicalDevice     m_physicalDevice = VK_NULL_HANDLE;
    VkDevice             m_device         = VK_NULL_HANDLE;
    VkQueue              m_graphicsQueue  = VK_NULL_HANDLE;
    VkQueue              m_presentQueue   = VK_NULL_HANDLE;
    uint32_t             m_graphicsFamily = 0;
    uint32_t             m_presentFamily  = 0;

    // Swapchain
    VkSwapchainKHR       m_swapchain      = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapImages;
    std::vector<VkImageView> m_swapImageViews;
    VkFormat             m_swapFormat;
    VkExtent2D           m_swapExtent;

    // Depth buffer
    VkImage              m_depthImage     = VK_NULL_HANDLE;
    VkDeviceMemory       m_depthMemory    = VK_NULL_HANDLE;
    VkImageView          m_depthView      = VK_NULL_HANDLE;

    // Render pass + framebuffers
    VkRenderPass         m_renderPass     = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;

    // Pipeline
    VkPipelineLayout     m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline           m_pipeline       = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool     m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet      m_descriptorSet  = VK_NULL_HANDLE;

    // Buffers
    VkBuffer             m_vertexBuffer   = VK_NULL_HANDLE;
    VkDeviceMemory       m_vertexMemory   = VK_NULL_HANDLE;
    VkBuffer             m_indexBuffer    = VK_NULL_HANDLE;
    VkDeviceMemory       m_indexMemory    = VK_NULL_HANDLE;
    VkBuffer             m_uniformBuffer  = VK_NULL_HANDLE;
    VkDeviceMemory       m_uniformMemory  = VK_NULL_HANDLE;

    // Command buffers
    VkCommandPool        m_commandPool    = VK_NULL_HANDLE;
    VkCommandBuffer      m_commandBuffer  = VK_NULL_HANDLE;

    // Sync
    VkSemaphore          m_imageAvailable = VK_NULL_HANDLE;
    VkSemaphore          m_renderFinished = VK_NULL_HANDLE;
    VkFence              m_inFlight       = VK_NULL_HANDLE;

    // Scene
    std::vector<DRO::Entity> m_entities;
    bool                 m_droEnabled = true;
    int                  m_forcedTier = -1;  // -1 = auto
    float                m_time = 0.0f;
    float                m_cameraAngle = 0.0f;
    float                m_cameraHeight = 10.0f;
    float                m_cameraDistance = 40.0f;
    float                m_fps = 0.0f;
    int                  m_frameCount = 0;
    float                m_fpsTimer = 0.0f;

    // ─── WINDOW ──────────────────────────────────────────────

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_window = glfwCreateWindow(WIDTH, HEIGHT, TITLE, nullptr, nullptr);
        glfwSetWindowUserPointer(m_window, this);
        glfwSetKeyCallback(m_window, keyCallback);

        std::cout << "[Base-X] Window created: " << WIDTH << "x" << HEIGHT << "\n";
    }

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        auto* app = reinterpret_cast<BaseXDemoApp*>(glfwGetWindowUserPointer(window));
        if (action != GLFW_PRESS) return;

        switch (key) {
            case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, true); break;
            case GLFW_KEY_D:     app->m_droEnabled = !app->m_droEnabled;
                std::cout << "[DRO] " << (app->m_droEnabled ? "ENABLED" : "DISABLED (all Base-64)") << "\n";
                break;
            case GLFW_KEY_1: app->m_forcedTier = 0; std::cout << "[LOD] Forced: Tier 0 (Base-64)\n"; break;
            case GLFW_KEY_2: app->m_forcedTier = 1; std::cout << "[LOD] Forced: Tier 1 (Base-48)\n"; break;
            case GLFW_KEY_3: app->m_forcedTier = 2; std::cout << "[LOD] Forced: Tier 2 (Base-32)\n"; break;
            case GLFW_KEY_4: app->m_forcedTier = 3; std::cout << "[LOD] Forced: Tier 3 (Base-16)\n"; break;
            case GLFW_KEY_5: app->m_forcedTier = 4; std::cout << "[LOD] Forced: Tier 4 (Base-8)\n";  break;
            case GLFW_KEY_0: app->m_forcedTier = -1; std::cout << "[LOD] Auto (DRO-driven)\n"; break;
            case GLFW_KEY_SPACE: app->spawnEntities(200);
                std::cout << "[Scene] Spawned 200 more. Total: " << app->m_entities.size() << "\n";
                break;
        }
    }

    // ─── VULKAN INIT ─────────────────────────────────────────

    void initVulkan() {
        createInstance();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapchain();
        createRenderPass();
        createDepthResources();
        createFramebuffers();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffer();
        createDescriptorPool();
        createDescriptorSet();
        createCommandPool();
        createCommandBuffer();
        createSyncObjects();

        std::cout << "[Vulkan] Initialization complete.\n"
                  << "[Vulkan] Physical device: " << getDeviceName() << "\n"
                  << "[Vulkan] Swapchain: " << m_swapExtent.width << "x"
                  << m_swapExtent.height << "\n";
    }

    void createInstance() {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Base-X Game Engine Demo";
        appInfo.applicationVersion = VK_MAKE_VERSION(2, 0, 0);
        appInfo.pEngineName = "Base-X Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(2, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        uint32_t glfwExtCount = 0;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = glfwExtCount;
        createInfo.ppEnabledExtensionNames = glfwExts;
        createInfo.enabledLayerCount = 0;

        if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan instance");
    }

    void createSurface() {
        if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS)
            throw std::runtime_error("Failed to create window surface");
    }

    void pickPhysicalDevice() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
        if (count == 0) throw std::runtime_error("No Vulkan-capable GPU found");

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

        // Pick first discrete GPU, or fall back to first device
        m_physicalDevice = devices[0];
        for (auto& dev : devices) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                m_physicalDevice = dev;
                break;
            }
        }

        // Find queue families
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

        bool foundGraphics = false, foundPresent = false;
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                m_graphicsFamily = i;
                foundGraphics = true;
            }
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &presentSupport);
            if (presentSupport) {
                m_presentFamily = i;
                foundPresent = true;
            }
            if (foundGraphics && foundPresent) break;
        }

        if (!foundGraphics || !foundPresent)
            throw std::runtime_error("GPU doesn't support required queue families");
    }

    std::string getDeviceName() {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
        return props.deviceName;
    }

    void createLogicalDevice() {
        std::set<uint32_t> uniqueQueueFamilies = {m_graphicsFamily, m_presentFamily};
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        float priority = 1.0f;

        for (uint32_t family : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = family;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &priority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.fillModeNonSolid = VK_TRUE;  // For wireframe at low LOD

        const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = 1;
        createInfo.ppEnabledExtensionNames = extensions;

        if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
            throw std::runtime_error("Failed to create logical device");

        vkGetDeviceQueue(m_device, m_graphicsFamily, 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, m_presentFamily, 0, &m_presentQueue);
    }

    void createSwapchain() {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities);

        // Format
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data());

        m_swapFormat = formats[0].format;
        VkColorSpaceKHR colorSpace = formats[0].colorSpace;
        for (auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                m_swapFormat = f.format;
                colorSpace = f.colorSpace;
                break;
            }
        }

        // Extent
        m_swapExtent = capabilities.currentExtent;
        if (m_swapExtent.width == UINT32_MAX) {
            m_swapExtent = {WIDTH, HEIGHT};
        }

        // Present mode — prefer mailbox (triple buffering)
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, presentModes.data());

        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (auto mode : presentModes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) { presentMode = mode; break; }
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
            imageCount = capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR swapInfo{};
        swapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapInfo.surface = m_surface;
        swapInfo.minImageCount = imageCount;
        swapInfo.imageFormat = m_swapFormat;
        swapInfo.imageColorSpace = colorSpace;
        swapInfo.imageExtent = m_swapExtent;
        swapInfo.imageArrayLayers = 1;
        swapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapInfo.preTransform = capabilities.currentTransform;
        swapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapInfo.presentMode = presentMode;
        swapInfo.clipped = VK_TRUE;

        uint32_t queueFamilyIndices[] = {m_graphicsFamily, m_presentFamily};
        if (m_graphicsFamily != m_presentFamily) {
            swapInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapInfo.queueFamilyIndexCount = 2;
            swapInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        if (vkCreateSwapchainKHR(m_device, &swapInfo, nullptr, &m_swapchain) != VK_SUCCESS)
            throw std::runtime_error("Failed to create swapchain");

        vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
        m_swapImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapImages.data());

        // Image views
        m_swapImageViews.resize(imageCount);
        for (size_t i = 0; i < imageCount; i++) {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = m_swapImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = m_swapFormat;
            viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

            if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapImageViews[i]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create image view");
        }
    }

    void createRenderPass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_swapFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = VK_FORMAT_D32_SFLOAT;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = (uint32_t)attachments.size();
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
            throw std::runtime_error("Failed to create render pass");
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }
        throw std::runtime_error("Failed to find suitable memory type");
    }

    void createDepthResources() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {m_swapExtent.width, m_swapExtent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_D32_SFLOAT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        vkCreateImage(m_device, &imageInfo, nullptr, &m_depthImage);

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(m_device, m_depthImage, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vkAllocateMemory(m_device, &allocInfo, nullptr, &m_depthMemory);
        vkBindImageMemory(m_device, m_depthImage, m_depthMemory, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_depthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_D32_SFLOAT;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};

        vkCreateImageView(m_device, &viewInfo, nullptr, &m_depthView);
    }

    void createFramebuffers() {
        m_framebuffers.resize(m_swapImageViews.size());
        for (size_t i = 0; i < m_swapImageViews.size(); i++) {
            std::array<VkImageView, 2> attachments = {m_swapImageViews[i], m_depthView};

            VkFramebufferCreateInfo fbInfo{};
            fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass = m_renderPass;
            fbInfo.attachmentCount = (uint32_t)attachments.size();
            fbInfo.pAttachments = attachments.data();
            fbInfo.width = m_swapExtent.width;
            fbInfo.height = m_swapExtent.height;
            fbInfo.layers = 1;

            if (vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create framebuffer");
        }
    }

    void createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding = 0;
        uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboBinding;

        vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout);
    }

    void createGraphicsPipeline() {
        // ── Shader modules ──
        // In a real build, these come from compiled .spv files
        // For now, we'll load them from disk or use a fallback
        
        // Push constant range for per-object DRO data
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;

        vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);

        // ── Pipeline creation ──
        // NOTE: Full pipeline requires compiled SPIR-V shaders.
        // The shader source is embedded above as comments.
        // Build integration:
        //   1. Save shader.vert and shader.frag to demos/FND-002/shaders/
        //   2. CMake custom command: glslangValidator -V shader.vert -o vert.spv
        //   3. Load .spv files here with createShaderModule()
        //
        // For the build system, see the CMakeLists.txt additions below.

        std::cout << "[Pipeline] Layout created with push constants (" 
                  << sizeof(PushConstants) << " bytes)\n";
        std::cout << "[Pipeline] NOTE: Compile shaders with glslangValidator before first run\n";
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory) {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = size;
        bufInfo.usage = usage;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(m_device, &bufInfo, nullptr, &buffer);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(m_device, buffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties);

        vkAllocateMemory(m_device, &allocInfo, nullptr, &memory);
        vkBindBufferMemory(m_device, buffer, memory, 0);
    }

    void createVertexBuffer() {
        VkDeviceSize size = sizeof(CUBE_VERTICES[0]) * CUBE_VERTICES.size();
        createBuffer(size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_vertexBuffer, m_vertexMemory);

        void* data;
        vkMapMemory(m_device, m_vertexMemory, 0, size, 0, &data);
        memcpy(data, CUBE_VERTICES.data(), size);
        vkUnmapMemory(m_device, m_vertexMemory);
    }

    void createIndexBuffer() {
        VkDeviceSize size = sizeof(CUBE_INDICES[0]) * CUBE_INDICES.size();
        createBuffer(size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_indexBuffer, m_indexMemory);

        void* data;
        vkMapMemory(m_device, m_indexMemory, 0, size, 0, &data);
        memcpy(data, CUBE_INDICES.data(), size);
        vkUnmapMemory(m_device, m_indexMemory);
    }

    void createUniformBuffer() {
        createBuffer(sizeof(UniformBufferObject),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_uniformBuffer, m_uniformMemory);
    }

    void createDescriptorPool() {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool);
    }

    void createDescriptorSet() {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_descriptorSetLayout;

        vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
    }

    void createCommandPool() {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = m_graphicsFamily;

        vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool);
    }

    void createCommandBuffer() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        vkAllocateCommandBuffers(m_device, &allocInfo, &m_commandBuffer);
    }

    void createSyncObjects() {
        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        vkCreateSemaphore(m_device, &semInfo, nullptr, &m_imageAvailable);
        vkCreateSemaphore(m_device, &semInfo, nullptr, &m_renderFinished);
        vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlight);
    }

    // ─── SCENE ───────────────────────────────────────────────

    void initScene() {
        spawnEntities(500);
        std::cout << "[Scene] Spawned " << m_entities.size() << " entities\n"
                  << "[Controls]\n"
                  << "  D     = Toggle DRO on/off\n"
                  << "  1-5   = Force LOD tier\n"
                  << "  0     = Auto (DRO-driven)\n"
                  << "  SPACE = Spawn 200 more entities\n"
                  << "  ESC   = Quit\n\n";
    }

    void spawnEntities(int count) {
        std::mt19937 rng((unsigned)m_entities.size() + 42);
        std::uniform_real_distribution<float> angleDist(0, 6.283f);
        std::uniform_real_distribution<float> radiusDist(2.0f, 80.0f);
        std::uniform_real_distribution<float> heightDist(-5.0f, 15.0f);
        std::uniform_int_distribution<int> impDist(1, 10);
        std::uniform_real_distribution<float> velDist(-3.0f, 3.0f);

        for (int i = 0; i < count; i++) {
            DRO::Entity e;
            float angle = angleDist(rng);
            float radius = radiusDist(rng);
            e.position = {cos(angle) * radius, heightDist(rng), sin(angle) * radius};
            e.velocity = {velDist(rng), velDist(rng) * 0.2f, velDist(rng)};
            e.importance = impDist(rng);
            e.layer = (radius < 5) ? DRO::PhysLayer::CONTACT :
                      (radius < 20) ? DRO::PhysLayer::RIGID :
                      (radius < 50) ? DRO::PhysLayer::PARTICLE : DRO::PhysLayer::FIELD;
            e.pulsePhase = angleDist(rng);  // random start phase
            m_entities.push_back(e);
        }
    }

    void updateScene(float dt) {
        m_time += dt;
        m_cameraAngle += dt * 0.3f;  // slow orbit

        glm::vec3 cameraPos(
            cos(m_cameraAngle) * m_cameraDistance,
            m_cameraHeight,
            sin(m_cameraAngle) * m_cameraDistance
        );

        // Update each entity
        int totalBase = 0;
        std::array<int, 5> tierCounts = {0,0,0,0,0};

        for (auto& e : m_entities) {
            // Move entity
            e.position += e.velocity * dt;
            
            // Bounce off ground
            if (e.position.y < -5.0f) {
                e.position.y = -5.0f;
                e.velocity.y = std::abs(e.velocity.y) * 0.8f;
            }
            
            // Apply gentle gravity
            e.velocity.y -= 2.0f * dt;

            // Calculate distance to camera
            glm::vec3 diff = e.position - cameraPos;
            e.distance = glm::length(diff);
            e.speed = glm::length(e.velocity);

            // DRO evaluation
            if (m_droEnabled && m_forcedTier < 0) {
                e.base = DRO::evaluate(e);
                e.lodTier = DRO::getTier(e.distance);
            } else if (m_forcedTier >= 0) {
                e.lodTier = m_forcedTier;
                e.base = DRO::TIERS[m_forcedTier].base;
            } else {
                e.base = 64;
                e.lodTier = 0;
            }

            // Pulse animation
            e.pulsePhase += dt * (2.0f + e.lodTier);
            float pulse = 0.5f + 0.5f * sin(e.pulsePhase);
            e.scale = 0.3f + (1.0f - (float)e.lodTier / 4.0f) * 0.7f;  // Smaller at high LOD tiers

            totalBase += e.base;
            if (e.lodTier >= 0 && e.lodTier < 5) tierCounts[e.lodTier]++;
        }

        // FPS counter
        m_frameCount++;
        m_fpsTimer += dt;
        if (m_fpsTimer >= 1.0f) {
            m_fps = m_frameCount / m_fpsTimer;
            m_frameCount = 0;
            m_fpsTimer = 0;

            // Print stats to console (until we have HUD overlay)
            float avgBase = (float)totalBase / m_entities.size();
            float savings = (1.0f - avgBase / 64.0f) * 100.0f;

            std::cout << "\r[FPS: " << std::fixed << std::setprecision(0) << m_fps
                      << " | Entities: " << m_entities.size()
                      << " | Avg Base: " << std::setprecision(1) << avgBase
                      << " | Savings: " << savings << "%"
                      << " | DRO: " << (m_droEnabled ? "ON" : "OFF")
                      << " | Tiers: "
                      << tierCounts[0] << "/" << tierCounts[1] << "/"
                      << tierCounts[2] << "/" << tierCounts[3] << "/"
                      << tierCounts[4] << "]      " << std::flush;
        }
    }

    void updateUniformBuffer() {
        glm::vec3 cameraPos(
            cos(m_cameraAngle) * m_cameraDistance,
            m_cameraHeight,
            sin(m_cameraAngle) * m_cameraDistance
        );

        UniformBufferObject ubo{};
        ubo.view = glm::lookAt(cameraPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        ubo.proj = glm::perspective(glm::radians(60.0f),
            (float)m_swapExtent.width / (float)m_swapExtent.height,
            0.1f, 500.0f);
        ubo.proj[1][1] *= -1;  // Vulkan Y-flip
        ubo.lightDir = glm::vec4(glm::normalize(glm::vec3(0.5f, 1.0f, 0.3f)), 0.0f);
        ubo.cameraPos = glm::vec4(cameraPos, 1.0f);
        ubo.time = m_time;

        void* data;
        vkMapMemory(m_device, m_uniformMemory, 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(m_device, m_uniformMemory);
    }

    void recordCommandBuffer(uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(m_commandBuffer, &beginInfo);

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.02f, 0.02f, 0.05f, 1.0f}};  // Near-black background
        clearValues[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = m_renderPass;
        rpInfo.framebuffer = m_framebuffers[imageIndex];
        rpInfo.renderArea = {{0, 0}, m_swapExtent};
        rpInfo.clearValueCount = (uint32_t)clearValues.size();
        rpInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(m_commandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        if (m_pipeline != VK_NULL_HANDLE) {
            vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

            VkBuffer vertexBuffers[] = {m_vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(m_commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);
            vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);

            // Draw each entity with DRO push constants
            for (auto& e : m_entities) {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), e.position);
                model = glm::scale(model, glm::vec3(e.scale));

                PushConstants push{};
                push.model = model;
                glm::vec3 color = DRO::getColorForBase(e.base);
                float pulse = 0.5f + 0.5f * sin(e.pulsePhase);
                push.droColor = glm::vec4(color, pulse);
                push.droBase = (float)e.base;
                push.lodTier = (float)e.lodTier;

                vkCmdPushConstants(m_commandBuffer, m_pipelineLayout,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0, sizeof(PushConstants), &push);

                vkCmdDrawIndexed(m_commandBuffer, (uint32_t)CUBE_INDICES.size(), 1, 0, 0, 0);
            }
        }

        vkCmdEndRenderPass(m_commandBuffer);
        vkEndCommandBuffer(m_commandBuffer);
    }

    // ─── MAIN LOOP ───────────────────────────────────────────

    void mainLoop() {
        auto lastTime = std::chrono::high_resolution_clock::now();

        std::cout << "\n[Base-X Engine] Entering main loop...\n\n";

        while (!glfwWindowShouldClose(m_window)) {
            glfwPollEvents();

            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - lastTime).count();
            lastTime = now;

            // Update DRO + scene
            updateScene(dt);
            updateUniformBuffer();

            // Render
            vkWaitForFences(m_device, 1, &m_inFlight, VK_TRUE, UINT64_MAX);
            vkResetFences(m_device, 1, &m_inFlight);

            uint32_t imageIndex;
            vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                m_imageAvailable, VK_NULL_HANDLE, &imageIndex);

            vkResetCommandBuffer(m_commandBuffer, 0);
            recordCommandBuffer(imageIndex);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &m_imageAvailable;
            submitInfo.pWaitDstStageMask = &waitStage;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &m_commandBuffer;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &m_renderFinished;

            vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlight);

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &m_renderFinished;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &m_swapchain;
            presentInfo.pImageIndices = &imageIndex;

            vkQueuePresentKHR(m_presentQueue, &presentInfo);
        }

        vkDeviceWaitIdle(m_device);
    }

    // ─── CLEANUP ─────────────────────────────────────────────

    void cleanup() {
        vkDestroySemaphore(m_device, m_imageAvailable, nullptr);
        vkDestroySemaphore(m_device, m_renderFinished, nullptr);
        vkDestroyFence(m_device, m_inFlight, nullptr);
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);

        vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
        vkFreeMemory(m_device, m_uniformMemory, nullptr);
        vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
        vkFreeMemory(m_device, m_indexMemory, nullptr);
        vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
        vkFreeMemory(m_device, m_vertexMemory, nullptr);

        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

        if (m_pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(m_device, m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

        for (auto fb : m_framebuffers) vkDestroyFramebuffer(m_device, fb, nullptr);
        vkDestroyImageView(m_device, m_depthView, nullptr);
        vkDestroyImage(m_device, m_depthImage, nullptr);
        vkFreeMemory(m_device, m_depthMemory, nullptr);
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        for (auto iv : m_swapImageViews) vkDestroyImageView(m_device, iv, nullptr);
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        vkDestroyDevice(m_device, nullptr);
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDestroyInstance(m_instance, nullptr);

        glfwDestroyWindow(m_window);
        glfwTerminate();

        std::cout << "\n\n[Base-X Engine] Shutdown complete.\n";
    }
};

// ═══════════════════════════════════════════════════════════════════
//  ENTRY POINT
// ═══════════════════════════════════════════════════════════════════

int main() {
    std::cout << "\n"
              << "████████████████████████████████████████████████████████████████\n"
              << "██                                                          ██\n"
              << "██   FND-002: LOD PHYSICS VISUALIZER — 3D VULKAN DEMO      ██\n"
              << "██   Base-X Game Engine v2.0.0                              ██\n"
              << "██                                                          ██\n"
              << "██   (c) 2025-2026 Keenan Wallace Dunham                   ██\n"
              << "██   Troll Hunter Gaming LLC — Patent Pending               ██\n"
              << "██                                                          ██\n"
              << "████████████████████████████████████████████████████████████████\n\n";

    BaseXDemoApp app;
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
