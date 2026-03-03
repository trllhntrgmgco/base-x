#ifndef BASEX_DRO_H
#define BASEX_DRO_H

#include <cstdint>
#include <cmath>
#include <vector>
#include <memory>

namespace BaseX {

// Radix range: Base-4 to Base-64
enum class RadixBase : uint8_t {
    BASE_4 = 4,
    BASE_8 = 8,
    BASE_16 = 16,
    BASE_32 = 32,
    BASE_64 = 64
};

// Precision requirements for different distance ranges
struct PrecisionLevel {
    float minDistance;      // Minimum distance in meters
    float maxDistance;      // Maximum distance in meters
    RadixBase radix;        // Optimal radix for this range
    uint32_t mantissaBits;  // Required mantissa precision
    float maxError;         // Maximum acceptable error in meters
};

// Dynamic Radix Optimization Manager
class DROManager {
public:
    DROManager();
    ~DROManager() = default;

    // Initialize with VR-optimized precision levels
    void initializeVRProfile();
    
    // Initialize with custom precision levels
    void initializeCustomProfile(const std::vector<PrecisionLevel>& levels);

    // Get optimal radix for given distance
    RadixBase getOptimalRadix(float distance) const;
    
    // Get precision level for distance
    const PrecisionLevel* getPrecisionLevel(float distance) const;

    // Check if precision is sufficient for operation
    bool validatePrecision(float distance, float requiredAccuracy) const;

    // Convert value between radix bases
    double convertRadix(double value, RadixBase fromBase, RadixBase toBase) const;

    // Get current performance metrics
    struct Metrics {
        uint64_t radixSwitches;
        uint64_t precisionWarnings;
        float averageError;
        RadixBase currentRadix;
    };
    Metrics getMetrics() const { return m_metrics; }

    // Reset metrics
    void resetMetrics();

private:
    std::vector<PrecisionLevel> m_precisionLevels;
    mutable Metrics m_metrics;
    
    // Helper functions
    double toRadix(double value, RadixBase base) const;
    double fromRadix(double value, RadixBase base) const;
};

// VR-Optimized Precision Profiles
namespace VRProfiles {
    // Sub-millimeter precision for hand-held objects (0-0.5m)
    constexpr PrecisionLevel HANDHELD = {
        0.0f,           // minDistance
        0.5f,           // maxDistance
        RadixBase::BASE_64,  // radix
        52,             // mantissaBits (double precision)
        0.0001f         // maxError (0.1mm)
    };

    // High precision for near interaction (0.5-2m)
    constexpr PrecisionLevel NEAR_FIELD = {
        0.5f,
        2.0f,
        RadixBase::BASE_32,
        48,
        0.001f          // 1mm error
    };

    // Standard precision for room-scale (2-10m)
    constexpr PrecisionLevel ROOM_SCALE = {
        2.0f,
        10.0f,
        RadixBase::BASE_16,
        32,
        0.01f           // 1cm error
    };

    // Medium precision for large spaces (10-100m)
    constexpr PrecisionLevel LARGE_SPACE = {
        10.0f,
        100.0f,
        RadixBase::BASE_8,
        24,
        0.1f            // 10cm error
    };

    // Low precision for distant objects (100m+)
    constexpr PrecisionLevel DISTANT = {
        100.0f,
        10000.0f,
        RadixBase::BASE_4,
        16,
        1.0f            // 1m error
    };
}

// LOD (Level of Detail) Manager integrated with DRO
class LODManager {
public:
    enum class LODLevel : uint8_t {
        LOD0_HIGHEST = 0,  // Closest, highest detail
        LOD1_HIGH = 1,
        LOD2_MEDIUM = 2,
        LOD3_LOW = 3,
        LOD4_LOWEST = 4    // Furthest, lowest detail
    };

    struct LODSettings {
        float distance;
        LODLevel level;
        RadixBase radix;
        float polyReduction;  // Polygon reduction factor
        bool enablePhysics;   // Whether to compute physics at this LOD
    };

    LODManager(DROManager* droManager);
    
    // Get appropriate LOD level for distance
    LODLevel getLODLevel(float distance) const;
    
    // Get LOD settings for distance
    LODSettings getLODSettings(float distance) const;
    
    // Update LOD thresholds
    void setLODThresholds(const std::vector<float>& thresholds);

private:
    DROManager* m_droManager;
    std::vector<float> m_lodThresholds;
};

} // namespace BaseX

#endif // BASEX_DRO_H
