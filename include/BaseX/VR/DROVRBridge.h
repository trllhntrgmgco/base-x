/**
 * @file DROVRBridge.h
 * @brief Base-X Game Engine — DRO ↔ VR Integration Bridge
 * 
 * Copyright (c) 2025-2026 Keenan Wallace Dunham / Troll Hunter Gaming LLC
 * All Rights Reserved. Patent Pending.
 * 
 * This bridge feeds VR tracking data INTO the DRO system so that:
 *   - Head pose drives distance-based precision for all entities
 *   - Hand poses trigger Base-64 for objects near the player's hands
 *   - Controller velocity feeds the velocity pillar
 *   - Foveation zones (future) create precision rings from eye tracking
 * 
 * This is THE key differentiator of Base-X: no other engine dynamically
 * adjusts physics AND rendering precision based on where you're looking
 * and what you're touching in VR.
 */

#pragma once

#include "VRSystem.h"

namespace BaseX {

// Forward declarations — avoid coupling to DRO headers
struct DROState;
class DynamicRadixOptimizer;

/**
 * @brief Configuration for how VR data influences DRO decisions
 */
struct DROVRConfig {
    // Distance thresholds (meters) for hand proximity boost
    float handContactDistance    = 0.15f;  // Within 15cm → Base-64
    float handInteractDistance   = 0.50f;  // Within 50cm → Base-48+
    float handNearDistance       = 1.50f;  // Within 1.5m → Base-32+

    // Velocity scaling for controller throw detection
    float velocityBoostThreshold = 2.0f;   // m/s — boost precision when moving fast
    float velocityMaxScale       = 10.0f;  // m/s — cap for velocity pillar

    // Gaze-based precision (future: eye tracking)
    float gazeInnerRadius        = 5.0f;   // degrees — foveal region
    float gazeMiddleRadius       = 15.0f;  // degrees — parafoveal
    float gazeOuterRadius        = 60.0f;  // degrees — peripheral

    int   gazeInnerBase          = 64;     // Foveal precision
    int   gazeMiddleBase         = 32;     // Parafoveal
    int   gazeOuterBase          = 16;     // Peripheral minimum

    // Haptic feedback tied to DRO transitions
    bool  hapticOnBaseChange     = true;   // Subtle buzz when touching high-precision object
    float hapticAmplitude        = 0.1f;   // Gentle pulse
    float hapticDuration         = 0.02f;  // 20ms
};

/**
 * @brief Per-frame VR data snapshot consumed by DRO
 */
struct DROVRSnapshot {
    // Head
    float headPosition[3]     = {0, 0, 0};
    float headForward[3]      = {0, 0, -1};

    // Left hand
    float leftHandPos[3]      = {0, 0, 0};
    float leftHandVelocity[3] = {0, 0, 0};
    bool  leftHandActive      = false;
    bool  leftTriggerHeld     = false;
    bool  leftGripHeld        = false;

    // Right hand
    float rightHandPos[3]     = {0, 0, 0};
    float rightHandVelocity[3]= {0, 0, 0};
    bool  rightHandActive     = false;
    bool  rightTriggerHeld    = false;
    bool  rightGripHeld       = false;

    // Derived
    float leftHandSpeed       = 0;  // magnitude of velocity
    float rightHandSpeed      = 0;
};

/**
 * @brief DRO ↔ VR Bridge
 * 
 * Each frame:
 *   1. Call updateFromVR() with current VRFrameState + VRInputState
 *   2. Call evaluateEntity() for each entity to get DRO influence from VR
 *   3. DRO combines this with other pillars (importance, physics layer, etc.)
 */
class DROVRBridge {
public:
    DROVRBridge();
    ~DROVRBridge();

    /**
     * @brief Configure VR-to-DRO parameters
     */
    void configure(const DROVRConfig& config);

    /**
     * @brief Update bridge with current VR frame data
     * 
     * Call once per frame after VRSystem::beginFrame().
     * Internally computes hand velocities via finite difference.
     * 
     * @param frame  Current frame state from VRSystem
     * @param input  Current input state from VRSystem
     * @param dt     Delta time in seconds
     */
    void updateFromVR(const VRFrameState& frame, const VRInputState& input, float dt);

    /**
     * @brief Get current VR snapshot (read-only, for DRO evaluation)
     */
    const DROVRSnapshot& getSnapshot() const;

    /**
     * @brief Evaluate VR influence on an entity's DRO state
     * 
     * Returns a base suggestion [4..64] based purely on VR proximity.
     * DRO core combines this with other pillars.
     * 
     * @param entityPos  Entity world position
     * @param entityRadius  Entity bounding radius (for contact detection)
     * @return Suggested base from VR proximity (0 = no VR influence)
     */
    int evaluateEntityProximity(const float entityPos[3], float entityRadius) const;

    /**
     * @brief Check if an entity is being held (within contact distance of a gripped hand)
     * 
     * @param entityPos    Entity world position
     * @param entityRadius Entity bounding radius
     * @return 0 = not held, 1 = left hand, 2 = right hand, 3 = both
     */
    int isEntityHeld(const float entityPos[3], float entityRadius) const;

    /**
     * @brief Should haptic fire for a DRO transition on this entity?
     * 
     * Returns which hand (0=none, 1=left, 2=right, 3=both) should
     * receive a haptic pulse when an entity near that hand changes base.
     */
    int shouldFireHaptic(const float entityPos[3], float entityRadius) const;

private:
    DROVRConfig   m_config;
    DROVRSnapshot m_snapshot;
    DROVRSnapshot m_prevSnapshot;   // for velocity computation
    bool          m_hasPrevFrame = false;

    float distanceSq(const float a[3], const float b[3]) const;
    float magnitude(const float v[3]) const;
};

} // namespace BaseX
