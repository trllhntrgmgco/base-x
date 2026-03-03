/**
 * @file VRSystem.cpp
 * @brief Base-X Game Engine — Complete OpenXR Implementation
 * 
 * Copyright (c) 2025-2026 Keenan Wallace Dunham / Troll Hunter Gaming LLC
 * All Rights Reserved. Patent Pending.
 * 
 * Implements the full OpenXR lifecycle with Vulkan graphics binding:
 *   - Instance creation with extension negotiation
 *   - System discovery and capability query
 *   - Session creation with Vulkan binding
 *   - Reference space (STAGE) creation
 *   - Stereo swapchain creation (one per eye)
 *   - Frame loop: xrWaitFrame → xrBeginFrame → acquire/render/release → xrEndFrame
 *   - Action sets and input bindings (Oculus, Index, Vive, WMR, Simple)
 *   - Haptic output
 *   - Graceful error handling and cleanup
 */

#include "VR/VRSystem.h"

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>       // XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>

#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cassert>

namespace BaseX {

// ═══════════════════════════════════════════════════════════════════
//  UTILITY MACROS
// ═══════════════════════════════════════════════════════════════════

#define XR_CHECK(call, msg)                                          \
    do {                                                             \
        XrResult _xr_result = (call);                                \
        if (XR_FAILED(_xr_result)) {                                 \
            char buf[XR_MAX_RESULT_STRING_SIZE];                     \
            xrResultToString(m_instance, _xr_result, buf);           \
            std::cerr << "[VR ERROR] " << msg << " — " << buf       \
                      << " (" << _xr_result << ")\n";                \
            return false;                                            \
        }                                                            \
    } while (0)

#define XR_CHECK_VOID(call, msg)                                     \
    do {                                                             \
        XrResult _xr_result = (call);                                \
        if (XR_FAILED(_xr_result)) {                                 \
            std::cerr << "[VR ERROR] " << msg                        \
                      << " (" << _xr_result << ")\n";                \
        }                                                            \
    } while (0)

// ═══════════════════════════════════════════════════════════════════
//  SWAPCHAIN WRAPPER
// ═══════════════════════════════════════════════════════════════════

struct SwapchainData {
    XrSwapchain                handle = XR_NULL_HANDLE;
    uint32_t                   width  = 0;
    uint32_t                   height = 0;
    std::vector<XrSwapchainImageVulkanKHR> images;
    uint32_t                   acquiredIndex = 0;
    bool                       acquired = false;
};

// ═══════════════════════════════════════════════════════════════════
//  IMPL (pImpl pattern — keeps OpenXR out of the header)
// ═══════════════════════════════════════════════════════════════════

struct VRSystem::Impl {
    // ── OpenXR core handles ─────────────────────────────────────
    XrInstance              instance        = XR_NULL_HANDLE;
    XrSystemId              systemId        = XR_NULL_SYSTEM_ID;
    XrSession               session         = XR_NULL_HANDLE;
    XrSpace                 stageSpace      = XR_NULL_HANDLE;
    XrSpace                 viewSpace       = XR_NULL_HANDLE;

    // ── Swapchains (one per eye) ────────────────────────────────
    SwapchainData           swapchains[2];
    VkFormat                swapchainFormat = VK_FORMAT_R8G8B8A8_SRGB;
    XrViewConfigurationView viewConfigs[2]  = {};

    // ── Frame state ─────────────────────────────────────────────
    XrFrameState            frameState      = {XR_TYPE_FRAME_STATE};
    XrView                  views[2]        = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    bool                    frameInProgress = false;
    bool                    sessionRunning  = false;

    // ── Session state ───────────────────────────────────────────
    VRSessionState          sessionState    = VRSessionState::Unknown;
    VRSessionStateCallback  stateCallback   = nullptr;

    // ── Input (action system) ───────────────────────────────────
    XrActionSet             actionSet       = XR_NULL_HANDLE;
    XrAction                gripPoseAction  = XR_NULL_HANDLE;
    XrAction                aimPoseAction   = XR_NULL_HANDLE;
    XrAction                triggerAction   = XR_NULL_HANDLE;
    XrAction                triggerClick    = XR_NULL_HANDLE;
    XrAction                gripAction      = XR_NULL_HANDLE;
    XrAction                gripClick       = XR_NULL_HANDLE;
    XrAction                thumbstickAction = XR_NULL_HANDLE;
    XrAction                thumbstickClick = XR_NULL_HANDLE;
    XrAction                primaryBtn      = XR_NULL_HANDLE;
    XrAction                secondaryBtn    = XR_NULL_HANDLE;
    XrAction                menuBtn         = XR_NULL_HANDLE;
    XrAction                hapticAction    = XR_NULL_HANDLE;
    XrSpace                 handSpaces[2]   = {XR_NULL_HANDLE, XR_NULL_HANDLE};  // grip
    XrSpace                 aimSpaces[2]    = {XR_NULL_HANDLE, XR_NULL_HANDLE};
    XrPath                  handPaths[2]    = {};

    // ── Vulkan references (not owned) ───────────────────────────
    VkInstance              vkInstance       = VK_NULL_HANDLE;
    VkPhysicalDevice        vkPhysicalDevice = VK_NULL_HANDLE;
    VkDevice                vkDevice         = VK_NULL_HANDLE;
    VkQueue                 vkQueue          = VK_NULL_HANDLE;
    uint32_t                vkQueueFamily    = 0;

    // ── Config ──────────────────────────────────────────────────
    VRConfig                config;

    // ── Runtime info ────────────────────────────────────────────
    std::string             runtimeName;
    std::string             systemName;

    // ── Methods ─────────────────────────────────────────────────
    bool createInstance();
    bool getSystem();
    bool createSession();
    bool createReferenceSpaces();
    bool createSwapchains();
    bool createActions();
    bool suggestBindings();
    void pollEvents();
    void handleSessionStateChange(XrSessionState newState);
    void transitionState(VRSessionState newState);
    void buildViewMatrix(const XrPosef& pose, float outMatrix[16]);
    void buildProjectionMatrix(const XrFovf& fov, float nearZ, float farZ, float outMatrix[16]);
    void xrPoseToVRPose(const XrPosef& xr, VRPose& out);
    void cleanup();
};

// ═══════════════════════════════════════════════════════════════════
//  CONSTRUCTOR / DESTRUCTOR / MOVE
// ═══════════════════════════════════════════════════════════════════

VRSystem::VRSystem() : m_impl(std::make_unique<Impl>()) {}

VRSystem::~VRSystem() {
    if (m_impl) shutdown();
}

VRSystem::VRSystem(VRSystem&& other) noexcept : m_impl(std::move(other.m_impl)) {}

VRSystem& VRSystem::operator=(VRSystem&& other) noexcept {
    if (this != &other) {
        if (m_impl) shutdown();
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

// ═══════════════════════════════════════════════════════════════════
//  INITIALIZE
// ═══════════════════════════════════════════════════════════════════

bool VRSystem::initialize(
    VkInstance       vulkanInstance,
    VkPhysicalDevice physicalDevice,
    VkDevice         device,
    VkQueue          graphicsQueue,
    uint32_t         queueFamilyIndex,
    const VRConfig&  config
) {
    m_impl->vkInstance       = vulkanInstance;
    m_impl->vkPhysicalDevice = physicalDevice;
    m_impl->vkDevice         = device;
    m_impl->vkQueue          = graphicsQueue;
    m_impl->vkQueueFamily    = queueFamilyIndex;
    m_impl->config           = config;

    std::cout << "[VR] Initializing Base-X VR System (OpenXR + Vulkan)\n";

    if (!m_impl->createInstance())       return false;
    if (!m_impl->getSystem())            return false;
    if (!m_impl->createSession())        return false;
    if (!m_impl->createReferenceSpaces()) return false;
    if (!m_impl->createSwapchains())     return false;
    if (!m_impl->createActions())        return false;
    if (!m_impl->suggestBindings())      return false;

    std::cout << "[VR] Initialization complete\n";
    std::cout << "[VR] Runtime: " << m_impl->runtimeName << "\n";
    std::cout << "[VR] System:  " << m_impl->systemName << "\n";
    std::cout << "[VR] Resolution: " << m_impl->swapchains[0].width
              << "x" << m_impl->swapchains[0].height << " per eye\n";

    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  INSTANCE CREATION
// ═══════════════════════════════════════════════════════════════════

bool VRSystem::Impl::createInstance() {
    // Enumerate available extensions
    uint32_t extCount = 0;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &extCount, nullptr);
    std::vector<XrExtensionProperties> exts(extCount, {XR_TYPE_EXTENSION_PROPERTIES});
    xrEnumerateInstanceExtensionProperties(nullptr, extCount, &extCount, exts.data());

    // Required: Vulkan graphics binding
    std::vector<const char*> enabledExts;
    bool hasVulkan = false;
    for (auto& ext : exts) {
        if (strcmp(ext.extensionName, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME) == 0) {
            enabledExts.push_back(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
            hasVulkan = true;
        }
        // Fallback to original Vulkan enable
        if (!hasVulkan && strcmp(ext.extensionName, XR_KHR_VULKAN_ENABLE_EXTENSION_NAME) == 0) {
            enabledExts.push_back(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);
            hasVulkan = true;
        }
    }

    if (!hasVulkan) {
        std::cerr << "[VR ERROR] OpenXR runtime does not support Vulkan graphics binding\n";
        return false;
    }

    // Optional extensions
    for (auto& ext : exts) {
        if (config.enableHandTracking &&
            strcmp(ext.extensionName, XR_EXT_HAND_TRACKING_EXTENSION_NAME) == 0) {
            enabledExts.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
            std::cout << "[VR] Hand tracking extension enabled\n";
        }
    }

    XrInstanceCreateInfo createInfo = {XR_TYPE_INSTANCE_CREATE_INFO};
    strncpy(createInfo.applicationInfo.applicationName,
            config.appName.c_str(),
            XR_MAX_APPLICATION_NAME_SIZE - 1);
    createInfo.applicationInfo.applicationVersion = config.appVersion;
    strncpy(createInfo.applicationInfo.engineName,
            "Base-X Engine",
            XR_MAX_ENGINE_NAME_SIZE - 1);
    createInfo.applicationInfo.engineVersion  = 2;  // v2.0
    createInfo.applicationInfo.apiVersion     = XR_CURRENT_API_VERSION;
    createInfo.enabledExtensionCount          = (uint32_t)enabledExts.size();
    createInfo.enabledExtensionNames          = enabledExts.data();

    XrResult result = xrCreateInstance(&createInfo, &instance);
    if (XR_FAILED(result)) {
        std::cerr << "[VR ERROR] Failed to create OpenXR instance (" << result << ")\n";
        return false;
    }

    // Query runtime info
    XrInstanceProperties props = {XR_TYPE_INSTANCE_PROPERTIES};
    xrGetInstanceProperties(instance, &props);
    runtimeName = props.runtimeName;

    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  SYSTEM DISCOVERY
// ═══════════════════════════════════════════════════════════════════

bool VRSystem::Impl::getSystem() {
    XrSystemGetInfo sysInfo = {XR_TYPE_SYSTEM_GET_INFO};
    sysInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    XR_CHECK(xrGetSystem(instance, &sysInfo, &systemId),
             "Failed to get HMD system");

    // Query system properties
    XrSystemProperties sysProps = {XR_TYPE_SYSTEM_PROPERTIES};
    XR_CHECK(xrGetSystemProperties(instance, systemId, &sysProps),
             "Failed to get system properties");

    systemName = sysProps.systemName;
    std::cout << "[VR] System: " << systemName
              << " (maxLayers=" << sysProps.graphicsProperties.maxLayerCount
              << ", maxSwapchain=" << sysProps.graphicsProperties.maxSwapchainImageWidth
              << "x" << sysProps.graphicsProperties.maxSwapchainImageHeight << ")\n";

    // Get view configuration (STEREO)
    uint32_t viewCount = 0;
    XR_CHECK(xrEnumerateViewConfigurationViews(
                 instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                 0, &viewCount, nullptr),
             "Failed to enumerate view configs");

    if (viewCount != 2) {
        std::cerr << "[VR ERROR] Expected 2 views (stereo), got " << viewCount << "\n";
        return false;
    }

    viewConfigs[0] = {XR_TYPE_VIEW_CONFIGURATION_VIEW};
    viewConfigs[1] = {XR_TYPE_VIEW_CONFIGURATION_VIEW};
    XR_CHECK(xrEnumerateViewConfigurationViews(
                 instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                 2, &viewCount, viewConfigs),
             "Failed to get view configuration views");

    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  SESSION CREATION (with Vulkan binding)
// ═══════════════════════════════════════════════════════════════════

bool VRSystem::Impl::createSession() {
    XrGraphicsBindingVulkanKHR vulkanBinding = {XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};
    vulkanBinding.instance          = vkInstance;
    vulkanBinding.physicalDevice    = vkPhysicalDevice;
    vulkanBinding.device            = vkDevice;
    vulkanBinding.queueFamilyIndex  = vkQueueFamily;
    vulkanBinding.queueIndex        = 0;

    XrSessionCreateInfo sessionInfo = {XR_TYPE_SESSION_CREATE_INFO};
    sessionInfo.next     = &vulkanBinding;
    sessionInfo.systemId = systemId;

    XR_CHECK(xrCreateSession(instance, &sessionInfo, &session),
             "Failed to create OpenXR session");

    std::cout << "[VR] Session created with Vulkan binding\n";
    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  REFERENCE SPACES
// ═══════════════════════════════════════════════════════════════════

bool VRSystem::Impl::createReferenceSpaces() {
    // STAGE space — room-scale, origin at floor center
    XrReferenceSpaceCreateInfo stageInfo = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    stageInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    stageInfo.poseInReferenceSpace.orientation = {0, 0, 0, 1};  // identity
    stageInfo.poseInReferenceSpace.position    = {0, 0, 0};

    XrResult result = xrCreateReferenceSpace(session, &stageInfo, &stageSpace);
    if (XR_FAILED(result)) {
        // Fallback to LOCAL if STAGE isn't available (seated headsets)
        std::cout << "[VR] STAGE space unavailable, falling back to LOCAL\n";
        stageInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        XR_CHECK(xrCreateReferenceSpace(session, &stageInfo, &stageSpace),
                 "Failed to create LOCAL reference space");
    }

    // VIEW space — locked to head, used for locating views
    XrReferenceSpaceCreateInfo viewInfo = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    viewInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    viewInfo.poseInReferenceSpace.orientation = {0, 0, 0, 1};
    viewInfo.poseInReferenceSpace.position    = {0, 0, 0};

    XR_CHECK(xrCreateReferenceSpace(session, &viewInfo, &viewSpace),
             "Failed to create VIEW reference space");

    std::cout << "[VR] Reference spaces created\n";
    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  SWAPCHAIN CREATION
// ═══════════════════════════════════════════════════════════════════

bool VRSystem::Impl::createSwapchains() {
    // Query supported formats
    uint32_t formatCount = 0;
    XR_CHECK(xrEnumerateSwapchainFormats(session, 0, &formatCount, nullptr),
             "Failed to enumerate swapchain formats");

    std::vector<int64_t> formats(formatCount);
    XR_CHECK(xrEnumerateSwapchainFormats(session, formatCount, &formatCount, formats.data()),
             "Failed to get swapchain formats");

    // Prefer SRGB for correct color space in VR
    VkFormat preferredFormats[] = {
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM
    };

    swapchainFormat = (VkFormat)formats[0];  // fallback
    if (config.colorFormat != 0) {
        swapchainFormat = config.colorFormat;
    } else {
        for (VkFormat pref : preferredFormats) {
            for (int64_t avail : formats) {
                if ((VkFormat)avail == pref) {
                    swapchainFormat = pref;
                    goto formatFound;
                }
            }
        }
    }
formatFound:

    std::cout << "[VR] Swapchain format: " << swapchainFormat << "\n";

    // Create one swapchain per eye
    for (int eye = 0; eye < 2; eye++) {
        SwapchainData& sc = swapchains[eye];
        sc.width  = viewConfigs[eye].recommendedImageRectWidth;
        sc.height = viewConfigs[eye].recommendedImageRectHeight;

        XrSwapchainCreateInfo scInfo = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
        scInfo.usageFlags  = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
                             XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        scInfo.format      = (int64_t)swapchainFormat;
        scInfo.sampleCount = viewConfigs[eye].recommendedSwapchainSampleCount;
        scInfo.width       = sc.width;
        scInfo.height      = sc.height;
        scInfo.faceCount   = 1;
        scInfo.arraySize   = 1;
        scInfo.mipCount    = 1;

        XR_CHECK(xrCreateSwapchain(session, &scInfo, &sc.handle),
                 "Failed to create swapchain for eye");

        // Get swapchain images
        uint32_t imgCount = 0;
        XR_CHECK(xrEnumerateSwapchainImages(sc.handle, 0, &imgCount, nullptr),
                 "Failed to enumerate swapchain images");

        sc.images.resize(imgCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
        XR_CHECK(xrEnumerateSwapchainImages(
                     sc.handle, imgCount, &imgCount,
                     reinterpret_cast<XrSwapchainImageBaseHeader*>(sc.images.data())),
                 "Failed to get swapchain images");

        std::cout << "[VR] Eye " << eye << " swapchain: "
                  << sc.width << "x" << sc.height
                  << " (" << imgCount << " images)\n";
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  ACTION SYSTEM (Input)
// ═══════════════════════════════════════════════════════════════════

bool VRSystem::Impl::createActions() {
    // Create action set
    XrActionSetCreateInfo setInfo = {XR_TYPE_ACTION_SET_CREATE_INFO};
    strncpy(setInfo.actionSetName, "basex_gameplay", XR_MAX_ACTION_SET_NAME_SIZE - 1);
    strncpy(setInfo.localizedActionSetName, "Base-X Gameplay", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE - 1);
    setInfo.priority = 0;

    XR_CHECK(xrCreateActionSet(instance, &setInfo, &actionSet),
             "Failed to create action set");

    // Hand subaction paths
    xrStringToPath(instance, "/user/hand/left",  &handPaths[0]);
    xrStringToPath(instance, "/user/hand/right", &handPaths[1]);

    // Helper lambda for action creation
    auto createAction = [&](XrAction& action, const char* name, const char* localName,
                            XrActionType type, bool perHand = true) -> bool {
        XrActionCreateInfo info = {XR_TYPE_ACTION_CREATE_INFO};
        info.actionType = type;
        strncpy(info.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
        strncpy(info.localizedActionName, localName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
        if (perHand) {
            info.countSubactionPaths = 2;
            info.subactionPaths     = handPaths;
        }
        XR_CHECK(xrCreateAction(actionSet, &info, &action),
                 (std::string("Failed to create action: ") + name).c_str());
        return true;
    };

    // Pose actions
    if (!createAction(gripPoseAction, "grip_pose", "Grip Pose", XR_ACTION_TYPE_POSE_INPUT)) return false;
    if (!createAction(aimPoseAction,  "aim_pose",  "Aim Pose",  XR_ACTION_TYPE_POSE_INPUT)) return false;

    // Float actions
    if (!createAction(triggerAction,    "trigger_value", "Trigger",    XR_ACTION_TYPE_FLOAT_INPUT)) return false;
    if (!createAction(gripAction,       "grip_value",    "Grip",       XR_ACTION_TYPE_FLOAT_INPUT)) return false;

    // Boolean actions
    if (!createAction(triggerClick,     "trigger_click", "Trigger Click", XR_ACTION_TYPE_BOOLEAN_INPUT)) return false;
    if (!createAction(gripClick,        "grip_click",    "Grip Click",    XR_ACTION_TYPE_BOOLEAN_INPUT)) return false;
    if (!createAction(primaryBtn,       "primary_btn",   "Primary",       XR_ACTION_TYPE_BOOLEAN_INPUT)) return false;
    if (!createAction(secondaryBtn,     "secondary_btn", "Secondary",     XR_ACTION_TYPE_BOOLEAN_INPUT)) return false;
    if (!createAction(thumbstickClick,  "stick_click",   "Stick Click",   XR_ACTION_TYPE_BOOLEAN_INPUT)) return false;
    if (!createAction(menuBtn,          "menu_btn",      "Menu",          XR_ACTION_TYPE_BOOLEAN_INPUT, false)) return false;

    // Vector2f action (thumbstick)
    if (!createAction(thumbstickAction, "thumbstick", "Thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT)) return false;

    // Haptic output
    if (!createAction(hapticAction, "haptic", "Haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT)) return false;

    // Create action spaces for hand tracking
    for (int hand = 0; hand < 2; hand++) {
        XrActionSpaceCreateInfo spaceInfo = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
        spaceInfo.action                        = gripPoseAction;
        spaceInfo.subactionPath                 = handPaths[hand];
        spaceInfo.poseInActionSpace.orientation  = {0, 0, 0, 1};
        XR_CHECK(xrCreateActionSpace(session, &spaceInfo, &handSpaces[hand]),
                 "Failed to create grip action space");

        spaceInfo.action = aimPoseAction;
        XR_CHECK(xrCreateActionSpace(session, &spaceInfo, &aimSpaces[hand]),
                 "Failed to create aim action space");
    }

    std::cout << "[VR] Action set created with all input bindings\n";
    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  INTERACTION PROFILE BINDINGS
// ═══════════════════════════════════════════════════════════════════

bool VRSystem::Impl::suggestBindings() {
    auto pathOf = [&](const char* str) -> XrPath {
        XrPath p;
        xrStringToPath(instance, str, &p);
        return p;
    };

    // ── Oculus Touch (Quest 2/3/Pro) ────────────────────────────
    {
        std::vector<XrActionSuggestedBinding> bindings = {
            {gripPoseAction,    pathOf("/user/hand/left/input/grip/pose")},
            {gripPoseAction,    pathOf("/user/hand/right/input/grip/pose")},
            {aimPoseAction,     pathOf("/user/hand/left/input/aim/pose")},
            {aimPoseAction,     pathOf("/user/hand/right/input/aim/pose")},
            {triggerAction,     pathOf("/user/hand/left/input/trigger/value")},
            {triggerAction,     pathOf("/user/hand/right/input/trigger/value")},
            {triggerClick,      pathOf("/user/hand/left/input/trigger/value")},
            {triggerClick,      pathOf("/user/hand/right/input/trigger/value")},
            {gripAction,        pathOf("/user/hand/left/input/squeeze/value")},
            {gripAction,        pathOf("/user/hand/right/input/squeeze/value")},
            {gripClick,         pathOf("/user/hand/left/input/squeeze/value")},
            {gripClick,         pathOf("/user/hand/right/input/squeeze/value")},
            {thumbstickAction,  pathOf("/user/hand/left/input/thumbstick")},
            {thumbstickAction,  pathOf("/user/hand/right/input/thumbstick")},
            {thumbstickClick,   pathOf("/user/hand/left/input/thumbstick/click")},
            {thumbstickClick,   pathOf("/user/hand/right/input/thumbstick/click")},
            {primaryBtn,        pathOf("/user/hand/left/input/x/click")},
            {primaryBtn,        pathOf("/user/hand/right/input/a/click")},
            {secondaryBtn,      pathOf("/user/hand/left/input/y/click")},
            {secondaryBtn,      pathOf("/user/hand/right/input/b/click")},
            {menuBtn,           pathOf("/user/hand/left/input/menu/click")},
            {hapticAction,      pathOf("/user/hand/left/output/haptic")},
            {hapticAction,      pathOf("/user/hand/right/output/haptic")},
        };

        XrInteractionProfileSuggestedBinding suggested = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggested.interactionProfile    = pathOf("/interaction_profiles/oculus/touch_controller");
        suggested.countSuggestedBindings = (uint32_t)bindings.size();
        suggested.suggestedBindings      = bindings.data();

        XrResult result = xrSuggestInteractionProfileBindings(instance, &suggested);
        if (XR_SUCCEEDED(result)) {
            std::cout << "[VR] Oculus Touch bindings registered\n";
        }
    }

    // ── Valve Index ─────────────────────────────────────────────
    {
        std::vector<XrActionSuggestedBinding> bindings = {
            {gripPoseAction,    pathOf("/user/hand/left/input/grip/pose")},
            {gripPoseAction,    pathOf("/user/hand/right/input/grip/pose")},
            {aimPoseAction,     pathOf("/user/hand/left/input/aim/pose")},
            {aimPoseAction,     pathOf("/user/hand/right/input/aim/pose")},
            {triggerAction,     pathOf("/user/hand/left/input/trigger/value")},
            {triggerAction,     pathOf("/user/hand/right/input/trigger/value")},
            {triggerClick,      pathOf("/user/hand/left/input/trigger/click")},
            {triggerClick,      pathOf("/user/hand/right/input/trigger/click")},
            {gripAction,        pathOf("/user/hand/left/input/squeeze/value")},
            {gripAction,        pathOf("/user/hand/right/input/squeeze/value")},
            {gripClick,         pathOf("/user/hand/left/input/squeeze/force")},
            {gripClick,         pathOf("/user/hand/right/input/squeeze/force")},
            {thumbstickAction,  pathOf("/user/hand/left/input/thumbstick")},
            {thumbstickAction,  pathOf("/user/hand/right/input/thumbstick")},
            {thumbstickClick,   pathOf("/user/hand/left/input/thumbstick/click")},
            {thumbstickClick,   pathOf("/user/hand/right/input/thumbstick/click")},
            {primaryBtn,        pathOf("/user/hand/left/input/a/click")},
            {primaryBtn,        pathOf("/user/hand/right/input/a/click")},
            {secondaryBtn,      pathOf("/user/hand/left/input/b/click")},
            {secondaryBtn,      pathOf("/user/hand/right/input/b/click")},
            {hapticAction,      pathOf("/user/hand/left/output/haptic")},
            {hapticAction,      pathOf("/user/hand/right/output/haptic")},
        };

        XrInteractionProfileSuggestedBinding suggested = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggested.interactionProfile     = pathOf("/interaction_profiles/valve/index_controller");
        suggested.countSuggestedBindings = (uint32_t)bindings.size();
        suggested.suggestedBindings      = bindings.data();

        XrResult result = xrSuggestInteractionProfileBindings(instance, &suggested);
        if (XR_SUCCEEDED(result)) {
            std::cout << "[VR] Valve Index bindings registered\n";
        }
    }

    // ── Simple Controller (fallback for any headset) ────────────
    {
        std::vector<XrActionSuggestedBinding> bindings = {
            {gripPoseAction,    pathOf("/user/hand/left/input/grip/pose")},
            {gripPoseAction,    pathOf("/user/hand/right/input/grip/pose")},
            {aimPoseAction,     pathOf("/user/hand/left/input/aim/pose")},
            {aimPoseAction,     pathOf("/user/hand/right/input/aim/pose")},
            {triggerClick,      pathOf("/user/hand/left/input/select/click")},
            {triggerClick,      pathOf("/user/hand/right/input/select/click")},
            {menuBtn,           pathOf("/user/hand/left/input/menu/click")},
            {hapticAction,      pathOf("/user/hand/left/output/haptic")},
            {hapticAction,      pathOf("/user/hand/right/output/haptic")},
        };

        XrInteractionProfileSuggestedBinding suggested = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggested.interactionProfile     = pathOf("/interaction_profiles/khr/simple_controller");
        suggested.countSuggestedBindings = (uint32_t)bindings.size();
        suggested.suggestedBindings      = bindings.data();

        XrResult result = xrSuggestInteractionProfileBindings(instance, &suggested);
        if (XR_SUCCEEDED(result)) {
            std::cout << "[VR] Simple controller bindings registered (fallback)\n";
        }
    }

    // Attach action set to session
    XrSessionActionSetsAttachInfo attachInfo = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets      = &actionSet;

    XR_CHECK(xrAttachSessionActionSets(session, &attachInfo),
             "Failed to attach action sets to session");

    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  EVENT POLLING & SESSION STATE
// ═══════════════════════════════════════════════════════════════════

void VRSystem::Impl::pollEvents() {
    XrEventDataBuffer event = {XR_TYPE_EVENT_DATA_BUFFER};

    while (xrPollEvent(instance, &event) == XR_SUCCESS) {
        switch (event.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                auto* stateEvent = reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
                handleSessionStateChange(stateEvent->state);
                break;
            }
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
                std::cout << "[VR] Instance loss pending — shutting down\n";
                transitionState(VRSessionState::LossPending);
                sessionRunning = false;
                break;
            }
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
                std::cout << "[VR] Interaction profile changed\n";
                break;
            }
            case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
                std::cout << "[VR] Reference space change pending\n";
                break;
            }
            default:
                break;
        }
        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

void VRSystem::Impl::handleSessionStateChange(XrSessionState newState) {
    std::cout << "[VR] Session state → " << newState << "\n";

    switch (newState) {
        case XR_SESSION_STATE_READY: {
            // Begin session
            XrSessionBeginInfo beginInfo = {XR_TYPE_SESSION_BEGIN_INFO};
            beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            XR_CHECK_VOID(xrBeginSession(session, &beginInfo), "Failed to begin session");
            sessionRunning = true;
            transitionState(VRSessionState::Ready);
            std::cout << "[VR] Session READY — entering frame loop\n";
            break;
        }
        case XR_SESSION_STATE_SYNCHRONIZED:
            transitionState(VRSessionState::Synchronized);
            break;
        case XR_SESSION_STATE_VISIBLE:
            transitionState(VRSessionState::Visible);
            break;
        case XR_SESSION_STATE_FOCUSED:
            transitionState(VRSessionState::Focused);
            std::cout << "[VR] Session FOCUSED — full input available\n";
            break;
        case XR_SESSION_STATE_STOPPING: {
            XR_CHECK_VOID(xrEndSession(session), "Failed to end session");
            sessionRunning = false;
            transitionState(VRSessionState::Stopping);
            std::cout << "[VR] Session STOPPING\n";
            break;
        }
        case XR_SESSION_STATE_EXITING:
            transitionState(VRSessionState::Exiting);
            sessionRunning = false;
            break;
        case XR_SESSION_STATE_LOSS_PENDING:
            transitionState(VRSessionState::LossPending);
            sessionRunning = false;
            break;
        default:
            break;
    }
}

void VRSystem::Impl::transitionState(VRSessionState newState) {
    VRSessionState old = sessionState;
    sessionState = newState;
    if (stateCallback) {
        stateCallback(old, newState);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  FRAME LOOP
// ═══════════════════════════════════════════════════════════════════

VRFrameState VRSystem::beginFrame() {
    VRFrameState result;
    auto& impl = *m_impl;

    // Poll events FIRST (handles session state transitions)
    impl.pollEvents();

    if (!impl.sessionRunning) {
        return result;
    }

    // xrWaitFrame — blocks until runtime says "render now"
    impl.frameState = {XR_TYPE_FRAME_STATE};
    XrFrameWaitInfo waitInfo = {XR_TYPE_FRAME_WAIT_INFO};
    XrResult xr = xrWaitFrame(impl.session, &waitInfo, &impl.frameState);
    if (XR_FAILED(xr)) {
        std::cerr << "[VR] xrWaitFrame failed (" << xr << ")\n";
        return result;
    }

    // xrBeginFrame — tell runtime we're starting
    XrFrameBeginInfo beginInfo = {XR_TYPE_FRAME_BEGIN_INFO};
    xr = xrBeginFrame(impl.session, &beginInfo);
    if (XR_FAILED(xr)) {
        std::cerr << "[VR] xrBeginFrame failed (" << xr << ")\n";
        return result;
    }

    impl.frameInProgress = true;
    result.shouldRender  = impl.frameState.shouldRender == XR_TRUE;
    result.predictedTime = impl.frameState.predictedDisplayTime;
    result.predictedDelta = impl.frameState.predictedDisplayPeriod;

    // Locate views (eye positions + FOVs)
    if (result.shouldRender) {
        XrViewLocateInfo locateInfo = {XR_TYPE_VIEW_LOCATE_INFO};
        locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locateInfo.displayTime           = impl.frameState.predictedDisplayTime;
        locateInfo.space                 = impl.stageSpace;

        XrViewState viewState = {XR_TYPE_VIEW_STATE};
        uint32_t viewCount = 2;
        xr = xrLocateViews(impl.session, &locateInfo, &viewState, 2, &viewCount, impl.views);
        if (XR_SUCCEEDED(xr) && (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT)) {
            // Use left eye as head approximation
            impl.xrPoseToVRPose(impl.views[0].pose, result.headPose);
            impl.buildViewMatrix(impl.views[0].pose, result.headViewMatrix);
        }

        // Sync actions (input)
        XrActiveActionSet activeSet = {};
        activeSet.actionSet = impl.actionSet;
        XrActionsSyncInfo syncInfo = {XR_TYPE_ACTIONS_SYNC_INFO};
        syncInfo.countActiveActionSets = 1;
        syncInfo.activeActionSets      = &activeSet;
        xrSyncActions(impl.session, &syncInfo);
    }

    return result;
}

VREyeTarget VRSystem::getEyeTarget(int eyeIndex) {
    VREyeTarget target;
    auto& impl = *m_impl;

    if (eyeIndex < 0 || eyeIndex > 1) return target;

    SwapchainData& sc = impl.swapchains[eyeIndex];

    // Acquire swapchain image
    XrSwapchainImageAcquireInfo acqInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    XrResult xr = xrAcquireSwapchainImage(sc.handle, &acqInfo, &sc.acquiredIndex);
    if (XR_FAILED(xr)) {
        std::cerr << "[VR] Failed to acquire swapchain image for eye " << eyeIndex << "\n";
        return target;
    }

    // Wait for image to be available
    XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;
    xr = xrWaitSwapchainImage(sc.handle, &waitInfo);
    if (XR_FAILED(xr)) {
        std::cerr << "[VR] Failed to wait for swapchain image\n";
        return target;
    }

    sc.acquired = true;

    // Fill target
    target.colorImage     = sc.images[sc.acquiredIndex].image;
    target.colorImageView = VK_NULL_HANDLE;  // Engine creates image views externally
    target.width          = sc.width;
    target.height         = sc.height;
    target.imageIndex     = sc.acquiredIndex;

    // Eye pose and matrices
    impl.xrPoseToVRPose(impl.views[eyeIndex].pose, target.eyePose);
    impl.buildViewMatrix(impl.views[eyeIndex].pose, target.viewMatrix);
    impl.buildProjectionMatrix(impl.views[eyeIndex].fov,
                               impl.config.nearPlane,
                               impl.config.farPlane,
                               target.projectionMatrix);

    // FOV
    target.fov.angleLeft  = impl.views[eyeIndex].fov.angleLeft;
    target.fov.angleRight = impl.views[eyeIndex].fov.angleRight;
    target.fov.angleUp    = impl.views[eyeIndex].fov.angleUp;
    target.fov.angleDown  = impl.views[eyeIndex].fov.angleDown;

    return target;
}

void VRSystem::releaseEyeTarget(int eyeIndex) {
    if (eyeIndex < 0 || eyeIndex > 1) return;

    SwapchainData& sc = m_impl->swapchains[eyeIndex];
    if (!sc.acquired) return;

    XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    xrReleaseSwapchainImage(sc.handle, &releaseInfo);
    sc.acquired = false;
}

void VRSystem::endFrame() {
    auto& impl = *m_impl;

    if (!impl.frameInProgress) return;

    // Build projection layers for both eyes
    XrCompositionLayerProjectionView projViews[2] = {};
    for (int eye = 0; eye < 2; eye++) {
        projViews[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        projViews[eye].pose = impl.views[eye].pose;
        projViews[eye].fov  = impl.views[eye].fov;
        projViews[eye].subImage.swapchain          = impl.swapchains[eye].handle;
        projViews[eye].subImage.imageRect.offset    = {0, 0};
        projViews[eye].subImage.imageRect.extent.width  = (int32_t)impl.swapchains[eye].width;
        projViews[eye].subImage.imageRect.extent.height = (int32_t)impl.swapchains[eye].height;
        projViews[eye].subImage.imageArrayIndex     = 0;
    }

    XrCompositionLayerProjection projLayer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    projLayer.space     = impl.stageSpace;
    projLayer.viewCount = 2;
    projLayer.views     = projViews;

    const XrCompositionLayerBaseHeader* layers[] = {
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projLayer)
    };

    XrFrameEndInfo endInfo = {XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime          = impl.frameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

    if (impl.frameState.shouldRender) {
        endInfo.layerCount = 1;
        endInfo.layers     = layers;
    } else {
        endInfo.layerCount = 0;
        endInfo.layers     = nullptr;
    }

    XrResult xr = xrEndFrame(impl.session, &endInfo);
    if (XR_FAILED(xr)) {
        std::cerr << "[VR] xrEndFrame failed (" << xr << ")\n";
    }

    impl.frameInProgress = false;
}

// ═══════════════════════════════════════════════════════════════════
//  INPUT STATE
// ═══════════════════════════════════════════════════════════════════

VRInputState VRSystem::getInputState() const {
    VRInputState state;
    auto& impl = *m_impl;

    if (!impl.sessionRunning) return state;

    auto readHand = [&](int idx, VRHand& hand) {
        // Grip pose
        XrSpaceLocation loc = {XR_TYPE_SPACE_LOCATION};
        if (XR_SUCCEEDED(xrLocateSpace(impl.handSpaces[idx], impl.stageSpace,
                                        impl.frameState.predictedDisplayTime, &loc))) {
            if (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
                impl.xrPoseToVRPose(loc.pose, hand.gripPose);
                hand.isActive = true;
            }
        }

        // Aim pose
        XrSpaceLocation aimLoc = {XR_TYPE_SPACE_LOCATION};
        if (XR_SUCCEEDED(xrLocateSpace(impl.aimSpaces[idx], impl.stageSpace,
                                        impl.frameState.predictedDisplayTime, &aimLoc))) {
            if (aimLoc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
                impl.xrPoseToVRPose(aimLoc.pose, hand.aimPose);
            }
        }

        // Trigger value
        XrActionStateFloat triggerState = {XR_TYPE_ACTION_STATE_FLOAT};
        XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action        = impl.triggerAction;
        getInfo.subactionPath = impl.handPaths[idx];
        if (XR_SUCCEEDED(xrGetActionStateFloat(impl.session, &getInfo, &triggerState))) {
            hand.triggerValue = triggerState.currentState;
        }

        // Trigger click
        XrActionStateBoolean trigClickState = {XR_TYPE_ACTION_STATE_BOOLEAN};
        getInfo.action = impl.triggerClick;
        if (XR_SUCCEEDED(xrGetActionStateBoolean(impl.session, &getInfo, &trigClickState))) {
            hand.triggerPressed = trigClickState.currentState;
        }

        // Grip value
        XrActionStateFloat gripState = {XR_TYPE_ACTION_STATE_FLOAT};
        getInfo.action = impl.gripAction;
        if (XR_SUCCEEDED(xrGetActionStateFloat(impl.session, &getInfo, &gripState))) {
            hand.gripValue = gripState.currentState;
        }

        // Grip click
        XrActionStateBoolean gripClickState = {XR_TYPE_ACTION_STATE_BOOLEAN};
        getInfo.action = impl.gripClick;
        if (XR_SUCCEEDED(xrGetActionStateBoolean(impl.session, &getInfo, &gripClickState))) {
            hand.gripPressed = gripClickState.currentState;
        }

        // Thumbstick
        XrActionStateVector2f stickState = {XR_TYPE_ACTION_STATE_VECTOR2F};
        getInfo.action = impl.thumbstickAction;
        if (XR_SUCCEEDED(xrGetActionStateVector2f(impl.session, &getInfo, &stickState))) {
            hand.thumbstickX = stickState.currentState.x;
            hand.thumbstickY = stickState.currentState.y;
        }

        // Thumbstick click
        XrActionStateBoolean stickClickState = {XR_TYPE_ACTION_STATE_BOOLEAN};
        getInfo.action = impl.thumbstickClick;
        if (XR_SUCCEEDED(xrGetActionStateBoolean(impl.session, &getInfo, &stickClickState))) {
            hand.thumbstickClick = stickClickState.currentState;
        }

        // Primary button (A/X)
        XrActionStateBoolean primaryState = {XR_TYPE_ACTION_STATE_BOOLEAN};
        getInfo.action = impl.primaryBtn;
        if (XR_SUCCEEDED(xrGetActionStateBoolean(impl.session, &getInfo, &primaryState))) {
            hand.primaryButton = primaryState.currentState;
        }

        // Secondary button (B/Y)
        XrActionStateBoolean secondaryState = {XR_TYPE_ACTION_STATE_BOOLEAN};
        getInfo.action = impl.secondaryBtn;
        if (XR_SUCCEEDED(xrGetActionStateBoolean(impl.session, &getInfo, &secondaryState))) {
            hand.secondaryButton = secondaryState.currentState;
        }
    };

    readHand(0, state.left);
    readHand(1, state.right);

    // Menu button (not per-hand)
    XrActionStateBoolean menuState = {XR_TYPE_ACTION_STATE_BOOLEAN};
    XrActionStateGetInfo menuInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    menuInfo.action = impl.menuBtn;
    if (XR_SUCCEEDED(xrGetActionStateBoolean(impl.session, &menuInfo, &menuState))) {
        state.left.menuButton = menuState.currentState;
    }

    return state;
}

// ═══════════════════════════════════════════════════════════════════
//  HAPTIC FEEDBACK
// ═══════════════════════════════════════════════════════════════════

void VRSystem::triggerHaptic(int hand, float duration, float frequency, float amplitude) {
    if (hand < 0 || hand > 1) return;
    if (!m_impl->sessionRunning) return;

    XrHapticVibration vibration = {XR_TYPE_HAPTIC_VIBRATION};
    vibration.amplitude = std::clamp(amplitude, 0.0f, 1.0f);
    vibration.frequency = frequency;
    vibration.duration  = (duration <= 0) ? XR_MIN_HAPTIC_DURATION
                                          : (XrDuration)(duration * 1e9);  // sec → nanosec

    XrHapticActionInfo hapticInfo = {XR_TYPE_HAPTIC_ACTION_INFO};
    hapticInfo.action        = m_impl->hapticAction;
    hapticInfo.subactionPath = m_impl->handPaths[hand];

    xrApplyHapticFeedback(m_impl->session, &hapticInfo,
                          reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
}

// ═══════════════════════════════════════════════════════════════════
//  QUERIES
// ═══════════════════════════════════════════════════════════════════

bool VRSystem::isRunning() const {
    return m_impl && m_impl->sessionRunning;
}

bool VRSystem::isFocused() const {
    return m_impl && m_impl->sessionState == VRSessionState::Focused;
}

void VRSystem::setSessionStateCallback(VRSessionStateCallback callback) {
    m_impl->stateCallback = std::move(callback);
}

VRSessionState VRSystem::getSessionState() const {
    return m_impl ? m_impl->sessionState : VRSessionState::Unknown;
}

std::string VRSystem::getRuntimeName() const {
    return m_impl ? m_impl->runtimeName : "";
}

std::string VRSystem::getSystemName() const {
    return m_impl ? m_impl->systemName : "";
}

void VRSystem::getRecommendedResolution(uint32_t& w, uint32_t& h) const {
    if (m_impl) {
        w = m_impl->viewConfigs[0].recommendedImageRectWidth;
        h = m_impl->viewConfigs[0].recommendedImageRectHeight;
    } else {
        w = h = 0;
    }
}

uint32_t VRSystem::getSwapchainImageCount() const {
    if (m_impl && !m_impl->swapchains[0].images.empty()) {
        return (uint32_t)m_impl->swapchains[0].images.size();
    }
    return 0;
}

std::vector<VkImage> VRSystem::getSwapchainImages(int eyeIndex) const {
    std::vector<VkImage> result;
    if (!m_impl || eyeIndex < 0 || eyeIndex > 1) return result;

    auto& sc = m_impl->swapchains[eyeIndex];
    result.reserve(sc.images.size());
    for (auto& img : sc.images) {
        result.push_back(img.image);
    }
    return result;
}

VkFormat VRSystem::getSwapchainFormat() const {
    return m_impl ? m_impl->swapchainFormat : (VkFormat)0;
}

// ═══════════════════════════════════════════════════════════════════
//  MATH HELPERS
// ═══════════════════════════════════════════════════════════════════

void VRSystem::Impl::xrPoseToVRPose(const XrPosef& xr, VRPose& out) {
    out.position[0] = xr.position.x;
    out.position[1] = xr.position.y;
    out.position[2] = xr.position.z;
    out.orientation[0] = xr.orientation.x;
    out.orientation[1] = xr.orientation.y;
    out.orientation[2] = xr.orientation.z;
    out.orientation[3] = xr.orientation.w;
    out.isValid = true;
}

void VRSystem::Impl::buildViewMatrix(const XrPosef& pose, float m[16]) {
    // Invert pose to get view matrix (camera looks from pose position)
    // Quaternion conjugate for rotation, negated rotated position for translation
    float qx = pose.orientation.x;
    float qy = pose.orientation.y;
    float qz = pose.orientation.z;
    float qw = pose.orientation.w;

    // Conjugate (inverse for unit quaternion)
    float ix = -qx, iy = -qy, iz = -qz, iw = qw;

    // Rotation matrix from inverse quaternion
    float xx = ix*ix, yy = iy*iy, zz = iz*iz;
    float xy = ix*iy, xz = ix*iz, yz = iy*iz;
    float wx = iw*ix, wy = iw*iy, wz = iw*iz;

    m[0]  = 1.0f - 2.0f*(yy + zz);
    m[1]  = 2.0f*(xy + wz);
    m[2]  = 2.0f*(xz - wy);
    m[3]  = 0.0f;

    m[4]  = 2.0f*(xy - wz);
    m[5]  = 1.0f - 2.0f*(xx + zz);
    m[6]  = 2.0f*(yz + wx);
    m[7]  = 0.0f;

    m[8]  = 2.0f*(xz + wy);
    m[9]  = 2.0f*(yz - wx);
    m[10] = 1.0f - 2.0f*(xx + yy);
    m[11] = 0.0f;

    // Translation: -R^T * position
    float px = pose.position.x;
    float py = pose.position.y;
    float pz = pose.position.z;
    m[12] = -(m[0]*px + m[4]*py + m[8]*pz);
    m[13] = -(m[1]*px + m[5]*py + m[9]*pz);
    m[14] = -(m[2]*px + m[6]*py + m[10]*pz);
    m[15] = 1.0f;
}

void VRSystem::Impl::buildProjectionMatrix(const XrFovf& fov, float nearZ, float farZ, float m[16]) {
    // Asymmetric frustum projection for VR
    // Vulkan NDC: Y down, Z [0,1]
    float tanL = tanf(fov.angleLeft);
    float tanR = tanf(fov.angleRight);
    float tanU = tanf(fov.angleUp);
    float tanD = tanf(fov.angleDown);

    float tanW = tanR - tanL;
    float tanH = tanU - tanD;  // Note: tanD is negative

    memset(m, 0, sizeof(float) * 16);

    m[0]  = 2.0f / tanW;
    m[5]  = 2.0f / tanH;
    m[8]  = (tanR + tanL) / tanW;
    m[9]  = (tanU + tanD) / tanH;

    if (farZ <= 0.0f || farZ == std::numeric_limits<float>::infinity()) {
        // Infinite far plane (recommended for VR — avoids precision issues)
        m[10] = -1.0f;
        m[14] = -nearZ;
    } else {
        m[10] = -(farZ) / (farZ - nearZ);
        m[14] = -(farZ * nearZ) / (farZ - nearZ);
    }
    m[11] = -1.0f;

    // Vulkan Y-flip (Vulkan NDC has Y pointing down)
    m[5] = -m[5];
    m[9] = -m[9];
}

// ═══════════════════════════════════════════════════════════════════
//  VRPose HELPERS
// ═══════════════════════════════════════════════════════════════════

void VRPose::getForward(float out[3]) const {
    // Forward = quaternion * (0, 0, -1)
    float qx = orientation[0], qy = orientation[1];
    float qz = orientation[2], qw = orientation[3];
    out[0] = 2.0f * (qx*qz + qw*qy);
    out[1] = 2.0f * (qy*qz - qw*qx);
    out[2] = -(1.0f - 2.0f*(qx*qx + qy*qy));
}

void VRPose::getRight(float out[3]) const {
    float qx = orientation[0], qy = orientation[1];
    float qz = orientation[2], qw = orientation[3];
    out[0] = 1.0f - 2.0f*(qy*qy + qz*qz);
    out[1] = 2.0f*(qx*qy + qw*qz);
    out[2] = 2.0f*(qx*qz - qw*qy);
}

void VRPose::getUp(float out[3]) const {
    float qx = orientation[0], qy = orientation[1];
    float qz = orientation[2], qw = orientation[3];
    out[0] = 2.0f*(qx*qy - qw*qz);
    out[1] = 1.0f - 2.0f*(qx*qx + qz*qz);
    out[2] = 2.0f*(qy*qz + qw*qx);
}

// ═══════════════════════════════════════════════════════════════════
//  SHUTDOWN & CLEANUP
// ═══════════════════════════════════════════════════════════════════

void VRSystem::shutdown() {
    if (!m_impl) return;
    m_impl->cleanup();
}

void VRSystem::Impl::cleanup() {
    std::cout << "[VR] Shutting down...\n";

    // Destroy action spaces
    for (int i = 0; i < 2; i++) {
        if (handSpaces[i] != XR_NULL_HANDLE) {
            xrDestroySpace(handSpaces[i]);
            handSpaces[i] = XR_NULL_HANDLE;
        }
        if (aimSpaces[i] != XR_NULL_HANDLE) {
            xrDestroySpace(aimSpaces[i]);
            aimSpaces[i] = XR_NULL_HANDLE;
        }
    }

    // Destroy swapchains
    for (int i = 0; i < 2; i++) {
        if (swapchains[i].handle != XR_NULL_HANDLE) {
            xrDestroySwapchain(swapchains[i].handle);
            swapchains[i].handle = XR_NULL_HANDLE;
        }
        swapchains[i].images.clear();
    }

    // Destroy spaces
    if (stageSpace != XR_NULL_HANDLE) {
        xrDestroySpace(stageSpace);
        stageSpace = XR_NULL_HANDLE;
    }
    if (viewSpace != XR_NULL_HANDLE) {
        xrDestroySpace(viewSpace);
        viewSpace = XR_NULL_HANDLE;
    }

    // Destroy action set (actions destroyed with it)
    if (actionSet != XR_NULL_HANDLE) {
        xrDestroyActionSet(actionSet);
        actionSet = XR_NULL_HANDLE;
    }

    // Destroy session
    if (session != XR_NULL_HANDLE) {
        xrDestroySession(session);
        session = XR_NULL_HANDLE;
    }

    // Destroy instance
    if (instance != XR_NULL_HANDLE) {
        xrDestroyInstance(instance);
        instance = XR_NULL_HANDLE;
    }

    sessionRunning = false;
    sessionState   = VRSessionState::Unknown;

    std::cout << "[VR] Shutdown complete\n";
}

} // namespace BaseX
