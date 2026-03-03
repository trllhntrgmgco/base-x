/**
 * @file VRSystem.h
 * @brief Base-X Game Engine — Public VR API
 * 
 * Copyright (c) 2025-2026 Keenan Wallace Dunham / Troll Hunter Gaming LLC
 * All Rights Reserved. Patent Pending.
 * 
 * This is the single public header for VR functionality.
 * It wraps OpenXR entirely — no OpenXR types leak into engine code.
 * 
 * Usage:
 *   BaseX::VRSystem vr;
 *   if (vr.initialize(vulkanInstance, physicalDevice, device, queue, queueFamily)) {
 *       while (vr.isRunning()) {
 *           VRFrameState frame = vr.beginFrame();
 *           if (frame.shouldRender) {
 *               for (int eye = 0; eye < 2; eye++) {
 *                   VREyeTarget target = vr.getEyeTarget(eye);
 *                   // Render scene to target.framebuffer with target.viewMatrix, target.projMatrix
 *                   vr.releaseEyeTarget(eye);
 *               }
 *           }
 *           vr.endFrame();
 *           // Read input
 *           VRInputState input = vr.getInputState();
 *       }
 *       vr.shutdown();
 *   }
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <functional>
#include <memory>

// Forward-declare Vulkan types so this header doesn't require vulkan.h
// These match the Vulkan spec typedefs
typedef struct VkInstance_T*       VkInstance;
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
typedef struct VkDevice_T*         VkDevice;
typedef struct VkQueue_T*          VkQueue;
typedef struct VkImage_T*          VkImage;
typedef struct VkImageView_T*      VkImageView;
typedef struct VkFramebuffer_T*    VkFramebuffer;
typedef uint32_t                   VkFormat;

namespace BaseX {

// ═══════════════════════════════════════════════════════════════════
//  DATA TYPES — No OpenXR leaks
// ═══════════════════════════════════════════════════════════════════

/**
 * @brief 3D pose (position + orientation)
 */
struct VRPose {
    float position[3]    = {0, 0, 0};      // meters, right-handed Y-up
    float orientation[4] = {0, 0, 0, 1};   // quaternion (x, y, z, w)
    bool  isValid        = false;

    // Convenience: extract forward direction from quaternion
    void getForward(float out[3]) const;
    void getRight(float out[3]) const;
    void getUp(float out[3]) const;
};

/**
 * @brief Per-eye field of view (asymmetric frustum)
 */
struct VRFov {
    float angleLeft   = 0;  // radians, negative
    float angleRight  = 0;  // radians, positive
    float angleUp     = 0;  // radians, positive
    float angleDown   = 0;  // radians, negative
};

/**
 * @brief Rendering target for one eye
 */
struct VREyeTarget {
    VkImage       colorImage     = nullptr;
    VkImageView   colorImageView = nullptr;
    uint32_t      width          = 0;
    uint32_t      height         = 0;
    uint32_t      imageIndex     = 0;
    VRPose        eyePose;
    VRFov         fov;
    float         viewMatrix[16];       // column-major 4x4
    float         projectionMatrix[16]; // column-major 4x4, infinite far plane
};

/**
 * @brief Frame timing and state from the VR runtime
 */
struct VRFrameState {
    bool     shouldRender     = false;   // false = skip rendering (minimized, etc.)
    int64_t  predictedTime    = 0;       // nanoseconds
    int64_t  predictedDelta   = 0;       // nanoseconds between frames
    VRPose   headPose;                   // predicted head position
    float    headViewMatrix[16];         // derived from headPose
};

/**
 * @brief Controller / hand state
 */
struct VRHand {
    VRPose  gripPose;                 // where the controller is
    VRPose  aimPose;                  // where the controller points
    float   triggerValue   = 0;       // 0.0 - 1.0
    float   gripValue      = 0;       // 0.0 - 1.0
    float   thumbstickX    = 0;       // -1.0 to 1.0
    float   thumbstickY    = 0;       // -1.0 to 1.0
    bool    triggerPressed = false;   // digital
    bool    gripPressed    = false;
    bool    primaryButton  = false;   // A/X
    bool    secondaryButton = false;  // B/Y
    bool    thumbstickClick = false;
    bool    menuButton     = false;
    bool    isActive       = false;   // controller tracked?
};

/**
 * @brief Combined input state for both hands
 */
struct VRInputState {
    VRHand left;
    VRHand right;
};

/**
 * @brief Session lifecycle events
 */
enum class VRSessionState {
    Unknown,
    Idle,
    Ready,
    Synchronized,
    Visible,
    Focused,
    Stopping,
    LossPending,
    Exiting
};

/**
 * @brief VR system configuration
 */
struct VRConfig {
    std::string appName    = "Base-X Engine";
    uint32_t    appVersion = 1;
    bool        enableDepthSubmission  = true;   // better reprojection
    bool        enableHandTracking     = false;  // XR_EXT_hand_tracking
    bool        enableFoveation        = false;  // XR_FB_foveation (Quest)
    VkFormat    colorFormat            = 0;      // 0 = auto-select (SRGB preferred)
    VkFormat    depthFormat            = 0;      // 0 = auto-select (D32)
    float       nearPlane              = 0.01f;  // meters
    float       farPlane               = 0.0f;   // 0 = infinite (recommended for VR)
};

/**
 * @brief Callback for session state changes
 */
using VRSessionStateCallback = std::function<void(VRSessionState oldState, VRSessionState newState)>;


// ═══════════════════════════════════════════════════════════════════
//  VR SYSTEM — Main interface
// ═══════════════════════════════════════════════════════════════════

/**
 * @brief Complete VR system wrapping OpenXR + Vulkan integration
 * 
 * Handles the full OpenXR lifecycle:
 *   Instance → System → Session → Swapchains → Frame loop → Input → Shutdown
 * 
 * DRO Integration Points:
 *   - headPose → drives distance-based DRO for all entities
 *   - hand gripPose → triggers Base-64 for held/touched objects
 *   - foveation data → future DRO precision zones from eye tracking
 *   - predictedDelta → feeds physics timestep
 */
class VRSystem {
public:
    VRSystem();
    ~VRSystem();

    // Non-copyable, movable
    VRSystem(const VRSystem&) = delete;
    VRSystem& operator=(const VRSystem&) = delete;
    VRSystem(VRSystem&& other) noexcept;
    VRSystem& operator=(VRSystem&& other) noexcept;

    // ── Lifecycle ───────────────────────────────────────────────

    /**
     * @brief Initialize OpenXR with Vulkan graphics binding
     * 
     * Creates XrInstance, discovers system, creates XrSession with Vulkan binding,
     * creates reference space, creates swapchains, and sets up action bindings.
     * 
     * @param vulkanInstance    Your existing VkInstance
     * @param physicalDevice    Your existing VkPhysicalDevice
     * @param device            Your existing VkDevice
     * @param graphicsQueue     Your existing VkQueue (graphics-capable)
     * @param queueFamilyIndex  Queue family index for graphicsQueue
     * @param config            VR configuration options
     * @return true on success
     */
    bool initialize(
        VkInstance       vulkanInstance,
        VkPhysicalDevice physicalDevice,
        VkDevice         device,
        VkQueue          graphicsQueue,
        uint32_t         queueFamilyIndex,
        const VRConfig&  config = {}
    );

    /**
     * @brief Clean shutdown — destroys all OpenXR resources
     */
    void shutdown();

    /**
     * @brief Is the VR system initialized and session active?
     */
    bool isRunning() const;

    /**
     * @brief Is the session in a focused state (user can interact)?
     */
    bool isFocused() const;

    /**
     * @brief Register callback for session state changes
     */
    void setSessionStateCallback(VRSessionStateCallback callback);

    // ── Frame Loop ──────────────────────────────────────────────

    /**
     * @brief Poll OpenXR events and begin a new frame
     * 
     * Call this ONCE per frame before any rendering.
     * Handles all event processing (session state changes, etc.)
     * and calls xrWaitFrame + xrBeginFrame.
     * 
     * @return Frame state. Check shouldRender before submitting images.
     */
    VRFrameState beginFrame();

    /**
     * @brief Get the rendering target for one eye
     * 
     * Acquires swapchain image, provides Vulkan image + view matrix.
     * You must call releaseEyeTarget() after rendering to this eye.
     * 
     * @param eyeIndex  0 = left, 1 = right
     * @return Eye target with Vulkan image and matrices
     */
    VREyeTarget getEyeTarget(int eyeIndex);

    /**
     * @brief Release eye target after rendering
     * 
     * Releases the swapchain image back to the OpenXR runtime.
     * Must be called once for each getEyeTarget() call.
     * 
     * @param eyeIndex  0 = left, 1 = right
     */
    void releaseEyeTarget(int eyeIndex);

    /**
     * @brief End frame — submit rendered images to VR runtime
     * 
     * Calls xrEndFrame with the projection layers for both eyes.
     * Must be called after rendering both eyes (or if shouldRender was false).
     */
    void endFrame();

    // ── Input ───────────────────────────────────────────────────

    /**
     * @brief Get current input state for both hands
     * 
     * Reads all action states (trigger, grip, thumbstick, buttons).
     * Call after beginFrame() for most up-to-date data.
     */
    VRInputState getInputState() const;

    /**
     * @brief Trigger haptic vibration on a controller
     * 
     * @param hand      0 = left, 1 = right
     * @param duration  Duration in seconds (0 = minimum pulse)
     * @param frequency Vibration frequency in Hz (0 = default)
     * @param amplitude 0.0 - 1.0
     */
    void triggerHaptic(int hand, float duration, float frequency, float amplitude);

    // ── Queries ─────────────────────────────────────────────────

    /**
     * @brief Get the recommended render resolution per eye
     */
    void getRecommendedResolution(uint32_t& outWidth, uint32_t& outHeight) const;

    /**
     * @brief Get the current session state
     */
    VRSessionState getSessionState() const;

    /**
     * @brief Get the runtime name (e.g., "Oculus", "SteamVR", "Monado")
     */
    std::string getRuntimeName() const;

    /**
     * @brief Get the system name (e.g., "Quest 3", "Valve Index")
     */
    std::string getSystemName() const;

    /**
     * @brief Get the number of swapchain images per eye
     */
    uint32_t getSwapchainImageCount() const;

    /**
     * @brief Get all swapchain images for an eye (for pre-creating framebuffers)
     */
    std::vector<VkImage> getSwapchainImages(int eyeIndex) const;

    /**
     * @brief Get the color format used by swapchains
     */
    VkFormat getSwapchainFormat() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace BaseX
