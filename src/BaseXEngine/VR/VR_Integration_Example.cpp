/**
 * @file VR_Integration_Example.cpp
 * @brief How to integrate VRSystem + DROVRBridge into the Base-X Engine loop
 * 
 * Copyright (c) 2025-2026 Keenan Wallace Dunham / Troll Hunter Gaming LLC
 * 
 * This is NOT a standalone demo — it shows the pattern for wiring VR into
 * your existing engine update loop. Copy the relevant sections into your
 * Engine::run() or main loop.
 * 
 * ═══════════════════════════════════════════════════════════════════
 *  CMake Addition — add to your existing CMakeLists.txt:
 * ═══════════════════════════════════════════════════════════════════
 * 
 *   # In your ENGINE_SOURCES list, add:
 *   src/BaseXEngine/VR/VRSystem.cpp
 *   src/BaseXEngine/VR/DROVRBridge.cpp
 * 
 *   # In your ENGINE_HEADERS list, add:
 *   include/BaseXEngine/VR/VRSystem.h
 *   include/BaseXEngine/VR/DROVRBridge.h
 * 
 *   # Find OpenXR:
 *   if(BASEX_BUILD_OPENXR)
 *       find_package(OpenXR REQUIRED)
 *       target_link_libraries(BaseXEngine PUBLIC OpenXR::openxr_loader)
 *       target_compile_definitions(BaseXEngine PUBLIC BASEX_VR_ENABLED)
 *   endif()
 * 
 *   # If OpenXR is installed via vcpkg:
 *   # vcpkg install openxr-loader
 * 
 * ═══════════════════════════════════════════════════════════════════
 */

#include "BaseXEngine/BaseXEngine.h"
#include "BaseXEngine/VR/VRSystem.h"
#include "BaseXEngine/VR/DROVRBridge.h"
#include "BaseXEngine/DRO/DynamicRadixOptimizer.h"
#include "BaseXEngine/Renderer/VulkanRenderer.h"

#include <iostream>
#include <chrono>

namespace BaseX {

/**
 * EXAMPLE: Engine main loop with VR + DRO integration
 * 
 * This shows the COMPLETE frame flow:
 *   1. VR beginFrame (poll events, wait for runtime, locate views)
 *   2. DRO bridge update (feed VR data into precision decisions)
 *   3. DRO evaluate all entities (combine VR proximity with other pillars)
 *   4. Physics step (at DRO-determined intervals)
 *   5. Render per-eye (with DRO push constants)
 *   6. VR endFrame (submit to compositor)
 */
void exampleVRLoop() {
    // ── 1. Create Vulkan renderer (you already have this) ───────
    VulkanRenderer renderer;
    RendererConfig renderCfg;
    renderCfg.vrMode = true;
    renderCfg.targetFPS = 90;
    renderer.initialize(renderCfg);

    // ── 2. Create VR system ─────────────────────────────────────
    VRSystem vr;
    VRConfig vrConfig;
    vrConfig.appName         = "Base-X Engine";
    vrConfig.nearPlane       = 0.01f;   // 1cm near plane for VR close-ups
    vrConfig.farPlane        = 0.0f;    // Infinite (recommended)
    vrConfig.enableHandTracking = false; // Enable when needed

    // Initialize with YOUR Vulkan handles
    // (get these from your VulkanRenderer after Vulkan init)
    /*
    if (!vr.initialize(
        renderer.getVkInstance(),
        renderer.getVkPhysicalDevice(),
        renderer.getVkDevice(),
        renderer.getVkGraphicsQueue(),
        renderer.getVkQueueFamilyIndex(),
        vrConfig
    )) {
        std::cerr << "Failed to init VR — running in desktop mode\n";
        // Fall back to desktop rendering
        return;
    }
    */

    // ── 3. Create DRO-VR bridge ─────────────────────────────────
    DROVRBridge droBridge;
    DROVRConfig droVRConfig;
    droVRConfig.handContactDistance  = 0.15f;  // 15cm = holding
    droVRConfig.handInteractDistance = 0.50f;  // 50cm = reaching
    droVRConfig.handNearDistance     = 1.50f;  // 1.5m = nearby
    droVRConfig.hapticOnBaseChange   = true;   // Buzz when touching high-precision objects
    droBridge.configure(droVRConfig);

    // ── 4. Register session state callback ──────────────────────
    vr.setSessionStateCallback([](VRSessionState oldState, VRSessionState newState) {
        std::cout << "[Engine] VR state: " << (int)oldState
                  << " → " << (int)newState << "\n";
        if (newState == VRSessionState::Focused) {
            std::cout << "[Engine] VR FOCUSED — full input active\n";
        }
    });

    // ── 5. Pre-create Vulkan image views for swapchain images ───
    /*
    for (int eye = 0; eye < 2; eye++) {
        auto images = vr.getSwapchainImages(eye);
        VkFormat format = vr.getSwapchainFormat();
        for (VkImage img : images) {
            // Create VkImageView for each swapchain image
            // Create VkFramebuffer for each image view
            // Store these for use during rendering
        }
    }
    */

    // ── 6. MAIN LOOP ────────────────────────────────────────────
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (vr.isRunning()) {
        // Delta time
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        // ── 6a. VR: Begin frame ────────────────────────────────
        VRFrameState frame = vr.beginFrame();

        // ── 6b. DRO Bridge: Feed VR data into DRO ─────────────
        VRInputState input = vr.getInputState();
        droBridge.updateFromVR(frame, input, dt);
        const DROVRSnapshot& vrSnap = droBridge.getSnapshot();

        // ── 6c. DRO: Evaluate all entities ─────────────────────
        // For each entity in the scene:
        /*
        for (auto& entity : scene.entities()) {
            DROState& dro = entity.getComponent<DROState>();
            
            // Get VR proximity base suggestion
            float pos[3] = {entity.position.x, entity.position.y, entity.position.z};
            int vrBase = droBridge.evaluateEntityProximity(pos, entity.boundingRadius);
            
            // If VR says high precision, override DRO minimum
            if (vrBase > 0) {
                dro.vrSuggestedBase = vrBase;
            }
            
            // Full DRO Five Pillars evaluation (includes VR data now)
            droOptimizer.evaluate(dro, entity, headPose, dt);
            
            // Haptic feedback on DRO transition
            int hapticHand = droBridge.shouldFireHaptic(pos, entity.boundingRadius);
            if (hapticHand > 0 && dro.baseJustChanged) {
                if (hapticHand & 1) vr.triggerHaptic(0, 0.02f, 0, 0.1f);
                if (hapticHand & 2) vr.triggerHaptic(1, 0.02f, 0, 0.1f);
            }
        }
        */

        // ── 6d. Physics: Step at DRO-determined intervals ──────
        // physicsWorld.step(dt, lodManager);

        // ── 6e. Render: Per-eye with DRO push constants ────────
        if (frame.shouldRender) {
            for (int eye = 0; eye < 2; eye++) {
                VREyeTarget target = vr.getEyeTarget(eye);

                // Transition swapchain image to COLOR_ATTACHMENT_OPTIMAL
                // renderer.transitionImage(target.colorImage, ...);

                // Begin render pass into the swapchain image
                // renderer.beginRenderPass(framebuffers[eye][target.imageIndex]);

                // Render scene with per-entity DRO push constants:
                /*
                for (auto& entity : scene.visibleEntities(target.viewMatrix, target.projectionMatrix)) {
                    DROState& dro = entity.getComponent<DROState>();
                    
                    // Push DRO state to shader
                    DROPushConstants pc;
                    pc.currentBase    = dro.currentBase;
                    pc.precisionScale = (float)dro.currentBase / 64.0f;
                    pc.lodBias        = lodManager.getMipBias(dro.currentBase);
                    renderer.pushConstants(pc);
                    
                    // Bind LOD-appropriate mesh + texture
                    LODTier tier = lodManager.getTier(entity);
                    renderer.bindDescriptorSet(tier.descriptorSet);
                    renderer.drawMesh(tier.mesh, entity.transform());
                }
                */

                // End render pass
                // renderer.endRenderPass();

                // Transition image back for compositor
                // renderer.transitionImage(target.colorImage, ...);

                vr.releaseEyeTarget(eye);
            }
        }

        // ── 6f. VR: End frame — submit to compositor ───────────
        vr.endFrame();

        // ── 6g. Debug: Print DRO stats ─────────────────────────
        /*
        if (frameCount % 90 == 0) {  // Once per second at 90 FPS
            std::cout << "[DRO] Head: ("
                << vrSnap.headPosition[0] << ", "
                << vrSnap.headPosition[1] << ", "
                << vrSnap.headPosition[2] << ")\n";
            if (vrSnap.leftHandActive) {
                std::cout << "[DRO] L-Hand speed: " << vrSnap.leftHandSpeed << " m/s\n";
            }
            if (vrSnap.rightHandActive) {
                std::cout << "[DRO] R-Hand speed: " << vrSnap.rightHandSpeed << " m/s\n";
            }
        }
        */
    }

    // ── 7. Cleanup ──────────────────────────────────────────────
    vr.shutdown();
    renderer.shutdown();
}

} // namespace BaseX

/**
 * ═══════════════════════════════════════════════════════════════════
 *  DRO-VR PRECISION FLOW SUMMARY
 * ═══════════════════════════════════════════════════════════════════
 * 
 *  Frame N:
 *    1. xrWaitFrame → predictedDisplayTime
 *    2. xrLocateViews → head + eye poses
 *    3. xrSyncActions → hand poses + input
 *    4. DROVRBridge::updateFromVR(frame, input, dt)
 *       → computes hand velocities
 *       → stores snapshot for DRO
 *    5. For each entity:
 *       a. DROVRBridge::evaluateEntityProximity(pos, radius)
 *          → returns base suggestion based on hand distance
 *       b. DRO Five Pillars combines:
 *          - VR proximity base (from bridge)
 *          - Distance from head (from bridge)
 *          - Entity importance (from game logic)
 *          - Entity velocity (from physics)
 *          - Physics layer (from entity type)
 *       c. Final base assigned → LOD tier selected
 *    6. Render per-eye with DRO push constants
 *    7. xrEndFrame → compositor takes over
 * 
 *  Result: Objects near your hands = Base-64 (sub-mm precision)
 *          Objects across the room = Base-8 (minimal precision)
 *          50-80% computation saved while VR stays at 90 FPS
 * 
 * ═══════════════════════════════════════════════════════════════════
 */
